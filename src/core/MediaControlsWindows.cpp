#include "MediaControls.hpp"

#include <QDebug>
#include <QDir>
#include <QGuiApplication>
#include <QMutex>
#include <QMutexLocker>
#include <QWindow>

#include <atomic>

// mingw-w64's windows.foundation.h (v13, at least) specialises IReference<> for both
// BYTE and boolean in C++ mode, and on Windows those are the same unsigned char, so the
// header does not compile as shipped. Pre-defining these include guards skips the
// boolean blocks, which nothing here uses. The Windows SDK spells its guards
// differently, so under MSVC the two lines are inert.
#define ____FIReference_1_boolean_INTERFACE_DEFINED__
#define ____FIReferenceArray_1_boolean_INTERFACE_DEFINED__

#include <windows.h>
#include <inspectable.h>
#include <roapi.h>
#include <winstring.h>
#include <windows.media.h>
#include <windows.storage.h>
#include <windows.storage.streams.h>
#include <systemmediatransportcontrolsinterop.h>

// System Media Transport Controls: the entry in the volume flyout and the lock screen,
// and the thing the keyboard's play/pause key talks to.
//
// Written against the raw WinRT ABI rather than C++/WinRT on purpose. C++/WinRT needs
// MSVC or clang; this app is also built with MinGW, whose headers ship the ABI
// interfaces and nothing more. The ABI is a plain COM vtable, so it works with both.
//
// Button presses arrive on a Windows Runtime thread. They are queued back onto the
// Qt thread before anything is emitted, so the rest of the app never sees a thread
// it did not create.

using namespace ABI::Windows::Media;
using ABI::Windows::Foundation::IAsyncOperation;
using ABI::Windows::Foundation::IAsyncOperationCompletedHandler;
using ABI::Windows::Foundation::ITypedEventHandler;

// mingw-w64 forward-declares IStorageFileStatics but never defines it, so the single
// method this file calls is declared here. It sits in the first slot after IInspectable,
// exactly as in the Windows SDK, and no other slot is ever touched. MSVC builds against
// the SDK's own definition, hence the guard.
#ifdef __MINGW32__
namespace ABI { namespace Windows { namespace Storage {

	struct IStorageFileStatics : public IInspectable
	{
		virtual HRESULT STDMETHODCALLTYPE	GetFileFromPathAsync(HSTRING path,
			IAsyncOperation<ABI::Windows::Storage::StorageFile *> **operation) = 0;
	};

} } }
#endif

namespace
{
	// By value rather than by the names the headers give them: MinGW and the Windows
	// SDK spell those differently, and this file has to build against both.
	const GUID	InteropIid =
		{0xddb0472d, 0xc911, 0x4a1f, {0x86, 0xd9, 0xdc, 0x3d, 0x71, 0xa9, 0x5f, 0x5a}};

	const GUID	ControlsIid =
		{0x99fa3ff4, 0x1742, 0x42a6, {0x90, 0x2e, 0x08, 0x7d, 0x41, 0xf9, 0x65, 0xec}};

	const GUID	ButtonHandlerIid =
		{0x0557e996, 0x7b23, 0x5bae, {0xaa, 0x81, 0xea, 0x0d, 0x67, 0x11, 0x43, 0xa4}};

	const GUID	StorageFileStaticsIid =
		{0x5984c710, 0xdaf2, 0x43c8, {0x8b, 0xb4, 0xa4, 0xd3, 0xea, 0xcf, 0xd0, 0x3f}};

	const GUID	StreamReferenceStaticsIid =
		{0x857309dc, 0x3fbf, 0x4e7d, {0x98, 0x6f, 0xef, 0x3b, 0x1a, 0x07, 0xa9, 0x64}};

	const GUID	FileHandlerIid =
		{0xe521c894, 0x2c26, 0x5946, {0x9e, 0x61, 0x2b, 0x5e, 0x18, 0x8d, 0x01, 0xed}};

	const wchar_t *const	ControlsClass = L"Windows.Media.SystemMediaTransportControls";
	const wchar_t *const	StorageFileClass = L"Windows.Storage.StorageFile";
	const wchar_t *const	StreamReferenceClass = L"Windows.Storage.Streams.RandomAccessStreamReference";

	template <class T>
	void	release(T *&object)
	{
		if (!object)
			return;

		object->Release();
		object = nullptr;
	}

	// An HSTRING that lives as long as the scope it was made in.
	class HString
	{
		public:
			explicit HString(const QString &text)
			{
				WindowsCreateString(reinterpret_cast<const WCHAR *>(text.utf16()),
					static_cast<UINT32>(text.size()), &_string);
			}

			~HString()
			{
				WindowsDeleteString(_string);
			}

			HString(const HString &) = delete;
			HString	&operator=(const HString &) = delete;

			HSTRING	get() const
			{
				return _string;
			}

		private:
			HSTRING	_string = nullptr;
	};

	class WindowsMediaControls;

	// The COM object Windows calls when a transport button is pressed. Reference
	// counted by hand because there is nothing else here that is.
	class ButtonHandler final
		: public ITypedEventHandler<SystemMediaTransportControls *,
			SystemMediaTransportControlsButtonPressedEventArgs *>
	{
		public:
			explicit ButtonHandler(WindowsMediaControls *owner)
				: _owner(owner)
			{
			}

			// Called by the owner on its way out. Windows may still hold a reference
			// and call Invoke afterwards; it then finds nobody home.
			void	detach()
			{
				QMutexLocker	lock(&_mutex);

				_owner = nullptr;
			}

			HRESULT STDMETHODCALLTYPE	QueryInterface(REFIID riid, void **object) override
			{
				if (!object)
					return E_POINTER;

				if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, ButtonHandlerIid))
				{
					*object = static_cast<IUnknown *>(this);
					AddRef();
					return S_OK;
				}

				*object = nullptr;
				return E_NOINTERFACE;
			}

			ULONG STDMETHODCALLTYPE	AddRef() override
			{
				return ++_references;
			}

			ULONG STDMETHODCALLTYPE	Release() override
			{
				ULONG	remaining = --_references;

				if (remaining == 0)
					delete this;

				return remaining;
			}

			HRESULT STDMETHODCALLTYPE	Invoke(ISystemMediaTransportControls *,
				ISystemMediaTransportControlsButtonPressedEventArgs *args) override;

		private:
			std::atomic<ULONG>		_references{1};
			QMutex					_mutex;
			WindowsMediaControls	*_owner;
	};

	// Opening the artwork file is asynchronous, so this is the callback that receives the
	// StorageFile. It arrives on a thread pool thread and hands the file straight back to
	// the Qt thread, where the display updater lives.
	class FileOpenedHandler final
		: public IAsyncOperationCompletedHandler<ABI::Windows::Storage::StorageFile *>
	{
		public:
			explicit FileOpenedHandler(WindowsMediaControls *owner)
				: _owner(owner)
			{
			}

			void	detach()
			{
				QMutexLocker	lock(&_mutex);

				_owner = nullptr;
			}

			HRESULT STDMETHODCALLTYPE	QueryInterface(REFIID riid, void **object) override
			{
				if (!object)
					return E_POINTER;

				if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, FileHandlerIid))
				{
					*object = static_cast<IUnknown *>(this);
					AddRef();
					return S_OK;
				}

				*object = nullptr;
				return E_NOINTERFACE;
			}

			ULONG STDMETHODCALLTYPE	AddRef() override
			{
				return ++_references;
			}

			ULONG STDMETHODCALLTYPE	Release() override
			{
				ULONG	remaining = --_references;

				if (remaining == 0)
					delete this;

				return remaining;
			}

			HRESULT STDMETHODCALLTYPE	Invoke(
				IAsyncOperation<ABI::Windows::Storage::StorageFile *> *operation,
				AsyncStatus status) override;

		private:
			std::atomic<ULONG>		_references{1};
			QMutex					_mutex;
			WindowsMediaControls	*_owner;
	};

	class WindowsMediaControls final : public MediaControls
	{
		public:
			explicit WindowsMediaControls(QObject *parent)
				: MediaControls(parent)
			{
			}

			~WindowsMediaControls() override
			{
				if (_controls && _handler)
					_controls->remove_ButtonPressed(_token);

				if (_handler)
				{
					_handler->detach();
					_handler->Release();
					_handler = nullptr;
				}

				if (_fileHandler)
				{
					_fileHandler->detach();
					_fileHandler->Release();
					_fileHandler = nullptr;
				}

				release(_updater);
				release(_controls);

				if (_ownsRuntime)
					RoUninitialize();
			}

			void	setEnabled(bool enabled) override
			{
				_enabled = enabled;

				if (initialise())
					applyEnabled();
			}

			void	setPlaybackState(PlaybackState state) override
			{
				_state = state;

				if (initialise())
					applyState();
			}

			void	setNowPlaying(const QString &title, const QString &artist) override
			{
				_title = title;
				_artist = artist;

				if (initialise())
					applyNowPlaying();
			}

			// Back on the Qt thread by the time this runs.
			void	handleButton(SystemMediaTransportControlsButton button)
			{
				switch (button)
				{
					case SystemMediaTransportControlsButton_Play:
						emit playRequested();
						break;
					case SystemMediaTransportControlsButton_Pause:
						emit pauseRequested();
						break;
					case SystemMediaTransportControlsButton_Stop:
						emit stopRequested();
						break;
					case SystemMediaTransportControlsButton_Next:
						emit nextRequested();
						break;
					case SystemMediaTransportControlsButton_Previous:
						emit previousRequested();
						break;
					default:
						break;
				}
			}

			void	setArtwork(const QString &path) override
			{
				QString	wanted = path.isEmpty() ? artworkPath() : path;

				if (wanted == _artwork)
					return;

				_artwork = wanted;

				if (initialise())
					requestThumbnail();
			}

			// Back on the Qt thread, with the opened file. Turns it into a stream
			// reference, hands it to the updater and republishes so the flyout picks the
			// picture up. Takes ownership of the reference it is handed.
			void	attachThumbnail(ABI::Windows::Storage::IStorageFile *file)
			{
				if (!file)
					return;

				if (_updater)
				{
					ABI::Windows::Storage::Streams::IRandomAccessStreamReferenceStatics	*statics = nullptr;

					HString	className(QString::fromWCharArray(StreamReferenceClass));

					if (SUCCEEDED(RoGetActivationFactory(className.get(), StreamReferenceStaticsIid,
						reinterpret_cast<void **>(&statics))) && statics)
					{
						ABI::Windows::Storage::Streams::IRandomAccessStreamReference	*reference = nullptr;

						if (SUCCEEDED(statics->CreateFromFile(file, &reference)) && reference)
						{
							_updater->put_Thumbnail(reference);
							_updater->Update();

							reference->Release();
						}

						statics->Release();
					}
				}

				file->Release();
			}

		private:
			bool	_initialised = false;
			bool	_failed = false;
			bool	_ownsRuntime = false;

			bool			_enabled = false;
			PlaybackState	_state = Stopped;
			QString			_title;
			QString			_artist;
			QString			_artwork = artworkPath();

			ISystemMediaTransportControls				*_controls = nullptr;
			ISystemMediaTransportControlsDisplayUpdater	*_updater = nullptr;
			ButtonHandler								*_handler = nullptr;
			FileOpenedHandler							*_fileHandler = nullptr;
			EventRegistrationToken						_token = {};

			// The controls are bound to a window, and the singleton that owns this
			// bridge is built before the window is. So the binding waits for the first
			// call that has something to show, by which time the window exists.
			bool	initialise()
			{
				if (_initialised)
					return true;

				if (_failed)
					return false;

				QWindow	*window = nullptr;

				for (QWindow *candidate : QGuiApplication::topLevelWindows())
				{
					if (candidate->handle())
					{
						window = candidate;
						break;
					}
				}

				// Not a failure, just too early. The next call tries again.
				if (!window)
					return false;

				HWND	hwnd = reinterpret_cast<HWND>(window->winId());

				// Qt has already put this thread in a single-threaded apartment, so this
				// answers S_FALSE. RPC_E_CHANGED_MODE would mean somebody chose the other
				// apartment first, which is also fine for what follows.
				HRESULT	hr = RoInitialize(RO_INIT_SINGLETHREADED);

				if (hr == S_OK)
					_ownsRuntime = true;
				else if (FAILED(hr) && hr != RPC_E_CHANGED_MODE)
					return fail("RoInitialize", hr);

				HString	className(QString::fromWCharArray(ControlsClass));

				ISystemMediaTransportControlsInterop	*interop = nullptr;

				hr = RoGetActivationFactory(className.get(), InteropIid,
					reinterpret_cast<void **>(&interop));

				if (FAILED(hr) || !interop)
					return fail("RoGetActivationFactory", hr);

				hr = interop->GetForWindow(hwnd, ControlsIid, reinterpret_cast<void **>(&_controls));
				interop->Release();

				if (FAILED(hr) || !_controls)
					return fail("GetForWindow", hr);

				hr = _controls->get_DisplayUpdater(&_updater);

				if (FAILED(hr) || !_updater)
					return fail("get_DisplayUpdater", hr);

				_updater->put_Type(MediaPlaybackType_Music);

				// Names the app in the flyout, alongside the version resource in the exe.
				HString	appMediaId(QStringLiteral("Pomodoro"));

				_updater->put_AppMediaId(appMediaId.get());

				requestThumbnail();

				// Next and previous move through a playlist, or between radio stations.
				_controls->put_IsPlayEnabled(true);
				_controls->put_IsPauseEnabled(true);
				_controls->put_IsStopEnabled(true);
				_controls->put_IsNextEnabled(true);
				_controls->put_IsPreviousEnabled(true);

				_handler = new ButtonHandler(this);

				hr = _controls->add_ButtonPressed(_handler, &_token);

				if (FAILED(hr))
				{
					_handler->Release();
					_handler = nullptr;

					return fail("add_ButtonPressed", hr);
				}

				_initialised = true;

				applyNowPlaying();
				applyState();
				applyEnabled();

				return true;
			}

			bool	fail(const char *what, HRESULT hr)
			{
				qWarning("pomodoro: media controls unavailable, %s failed with 0x%08lx",
					what, static_cast<unsigned long>(hr));

				_failed = true;

				release(_updater);
				release(_controls);

				return false;
			}

			void	applyEnabled()
			{
				_controls->put_IsEnabled(_enabled);

				// Disabled controls linger in the flyout until their status says closed.
				if (!_enabled)
					_controls->put_PlaybackStatus(MediaPlaybackStatus_Closed);
				else
					applyState();
			}

			void	applyState()
			{
				if (!_enabled)
					return;

				MediaPlaybackStatus	status = MediaPlaybackStatus_Stopped;

				if (_state == Playing)
					status = MediaPlaybackStatus_Playing;
				else if (_state == Paused)
					status = MediaPlaybackStatus_Paused;

				_controls->put_PlaybackStatus(status);
			}

			// The artwork shown beside the title: the song's cover when MusicPlayer has one
			// (setArtwork), else the app's logo -- a radio stream carries no cover of its own,
			// ICY metadata has no field for one.
			//
			// put_Thumbnail needs an IRandomAccessStreamReference, and building one from a
			// plain path goes through StorageFile, which is asynchronous. This only starts the
			// request; attachThumbnail finishes once the file is open. CreateFromUri on a
			// file:// URI looks like a shortcut and is not one: it hands back a reference that
			// fails on the first read, so the flyout ends up showing no picture at all.
			void	requestThumbnail()
			{
				QString	path = QDir::toNativeSeparators(_artwork);

				if (path.isEmpty())
					return;

				// A newer picture replaces one still opening: the old handler is cut loose,
				// so a slow earlier file can never land on top of the current one.
				if (_fileHandler)
				{
					_fileHandler->detach();
					_fileHandler->Release();
					_fileHandler = nullptr;
				}

				ABI::Windows::Storage::IStorageFileStatics	*statics = nullptr;

				HString	className(QString::fromWCharArray(StorageFileClass));

				if (FAILED(RoGetActivationFactory(className.get(), StorageFileStaticsIid,
					reinterpret_cast<void **>(&statics))) || !statics)
				{
					return;
				}

				HString	pathValue(path);

				IAsyncOperation<ABI::Windows::Storage::StorageFile *>	*operation = nullptr;

				HRESULT	hr = statics->GetFileFromPathAsync(pathValue.get(), &operation);

				statics->Release();

				if (FAILED(hr) || !operation)
					return;

				_fileHandler = new FileOpenedHandler(this);

				if (FAILED(operation->put_Completed(_fileHandler)))
				{
					_fileHandler->detach();
					_fileHandler->Release();
					_fileHandler = nullptr;
				}

				operation->Release();
			}

			void	applyNowPlaying()
			{
				IMusicDisplayProperties	*music = nullptr;

				if (FAILED(_updater->get_MusicProperties(&music)) || !music)
					return;

				HString	title(_title);
				HString	artist(_artist);

				music->put_Title(title.get());
				music->put_Artist(artist.get());
				music->Release();

				// Nothing shows until the updater is told the properties are complete.
				_updater->Update();
			}
	};

	HRESULT STDMETHODCALLTYPE	FileOpenedHandler::Invoke(
		IAsyncOperation<ABI::Windows::Storage::StorageFile *> *operation, AsyncStatus status)
	{
		if (!operation || status != Completed)
			return S_OK;

		ABI::Windows::Storage::IStorageFile	*file = nullptr;

		if (FAILED(operation->GetResults(&file)) || !file)
			return S_OK;

		QMutexLocker	lock(&_mutex);

		if (!_owner)
		{
			file->Release();
			return S_OK;
		}

		// The reference from GetResults is passed on to the queued call, which releases it
		// once the updater has taken one of its own.
		WindowsMediaControls	*owner = _owner;

		QMetaObject::invokeMethod(owner, [owner, file]()
		{
			owner->attachThumbnail(file);
		}, Qt::QueuedConnection);

		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE	ButtonHandler::Invoke(ISystemMediaTransportControls *,
		ISystemMediaTransportControlsButtonPressedEventArgs *args)
	{
		SystemMediaTransportControlsButton	button = SystemMediaTransportControlsButton_Play;

		if (!args || FAILED(args->get_Button(&button)))
			return S_OK;

		QMutexLocker	lock(&_mutex);

		// Queued onto the owner's thread. If the owner dies before the call is
		// delivered, Qt drops it along with the rest of the owner's pending events.
		if (_owner)
		{
			WindowsMediaControls	*owner = _owner;

			QMetaObject::invokeMethod(owner, [owner, button]()
			{
				owner->handleButton(button);
			}, Qt::QueuedConnection);
		}

		return S_OK;
	}
}

MediaControls	*createPlatformMediaControls(QObject *parent)
{
	return new WindowsMediaControls(parent);
}

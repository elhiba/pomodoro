#include "MediaControls.hpp"

#include <QImage>
#include <QMetaObject>
#include <QString>

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>
#import <MediaPlayer/MediaPlayer.h>

// The Now Playing centre: the entry in Control Centre and the menu bar, and what the
// keyboard's play/pause key and AirPods taps are routed to.
//
// Two halves. MPNowPlayingInfoCenter is told what is on and whether it is playing;
// MPRemoteCommandCenter is where the play, pause, toggle and stop commands come back
// in. The command handlers run on the main thread already, but they are still queued
// rather than emitted directly so nothing runs inside AppKit's callback.

namespace
{
	const char *const	IconResource = ":/qt/qml/Pomodoro/assets/icons/pomodoroLogoTransport.png";

	class MacMediaControls final : public MediaControls
	{
		public:
			explicit MacMediaControls(QObject *parent)
				: MediaControls(parent)
			{
				MPRemoteCommandCenter	*commands = [MPRemoteCommandCenter sharedCommandCenter];

				_playToken = [commands.playCommand addTargetWithHandler:^MPRemoteCommandHandlerStatus(MPRemoteCommandEvent *event)
				{
					(void)event;
					relay(&MediaControls::playRequested);
					return MPRemoteCommandHandlerStatusSuccess;
				}];

				_pauseToken = [commands.pauseCommand addTargetWithHandler:^MPRemoteCommandHandlerStatus(MPRemoteCommandEvent *event)
				{
					(void)event;
					relay(&MediaControls::pauseRequested);
					return MPRemoteCommandHandlerStatusSuccess;
				}];

				_toggleToken = [commands.togglePlayPauseCommand addTargetWithHandler:^MPRemoteCommandHandlerStatus(MPRemoteCommandEvent *event)
				{
					(void)event;
					relay(&MediaControls::toggleRequested);
					return MPRemoteCommandHandlerStatusSuccess;
				}];

				_stopToken = [commands.stopCommand addTargetWithHandler:^MPRemoteCommandHandlerStatus(MPRemoteCommandEvent *event)
				{
					(void)event;
					relay(&MediaControls::stopRequested);
					return MPRemoteCommandHandlerStatusSuccess;
				}];

				// A live stream: nothing to skip to and nothing to seek in.
				commands.nextTrackCommand.enabled = NO;
				commands.previousTrackCommand.enabled = NO;
				commands.changePlaybackPositionCommand.enabled = NO;
				commands.seekForwardCommand.enabled = NO;
				commands.seekBackwardCommand.enabled = NO;

				loadArtwork();
				applyEnabled();
			}

			~MacMediaControls() override
			{
				MPRemoteCommandCenter	*commands = [MPRemoteCommandCenter sharedCommandCenter];

				[commands.playCommand removeTarget:_playToken];
				[commands.pauseCommand removeTarget:_pauseToken];
				[commands.togglePlayPauseCommand removeTarget:_toggleToken];
				[commands.stopCommand removeTarget:_stopToken];

				[MPNowPlayingInfoCenter defaultCenter].nowPlayingInfo = nil;
				[MPNowPlayingInfoCenter defaultCenter].playbackState = MPNowPlayingPlaybackStateStopped;

#if !__has_feature(objc_arc)
				[_artwork release];
#endif
			}

			void	setEnabled(bool enabled) override
			{
				if (_enabled == enabled)
					return;

				_enabled = enabled;
				applyEnabled();
			}

			void	setPlaybackState(PlaybackState state) override
			{
				_state = state;
				applyInfo();
			}

			void	setNowPlaying(const QString &title, const QString &artist) override
			{
				_title = title;
				_artist = artist;
				applyInfo();
			}

		private:
			bool			_enabled = false;
			PlaybackState	_state = Stopped;
			QString			_title;
			QString			_artist;

			id	_playToken = nil;
			id	_pauseToken = nil;
			id	_toggleToken = nil;
			id	_stopToken = nil;

			MPMediaItemArtwork	*_artwork = nil;

			// Onto the Qt event loop, with the member function pointer naming which
			// signal to raise once it gets there.
			void	relay(void (MediaControls::*signal)())
			{
				QMetaObject::invokeMethod(this, [this, signal]()
				{
					emit (this->*signal)();
				}, Qt::QueuedConnection);
			}

			void	applyEnabled()
			{
				MPRemoteCommandCenter	*commands = [MPRemoteCommandCenter sharedCommandCenter];

				commands.playCommand.enabled = _enabled;
				commands.pauseCommand.enabled = _enabled;
				commands.togglePlayPauseCommand.enabled = _enabled;
				commands.stopCommand.enabled = _enabled;

				applyInfo();
			}

			void	applyInfo()
			{
				MPNowPlayingInfoCenter	*center = [MPNowPlayingInfoCenter defaultCenter];

				if (!_enabled)
				{
					center.nowPlayingInfo = nil;
					center.playbackState = MPNowPlayingPlaybackStateStopped;
					return;
				}

				NSMutableDictionary	*info = [NSMutableDictionary dictionary];

				info[MPMediaItemPropertyTitle] = _title.toNSString();
				info[MPMediaItemPropertyArtist] = _artist.toNSString();
				info[MPNowPlayingInfoPropertyIsLiveStream] = @YES;
				info[MPNowPlayingInfoPropertyPlaybackRate] = @(_state == Playing ? 1.0 : 0.0);

				if (_artwork)
					info[MPMediaItemPropertyArtwork] = _artwork;

				center.nowPlayingInfo = info;

				switch (_state)
				{
					case Playing:
						center.playbackState = MPNowPlayingPlaybackStatePlaying;
						break;
					case Paused:
						center.playbackState = MPNowPlayingPlaybackStatePaused;
						break;
					case Stopped:
					default:
						center.playbackState = MPNowPlayingPlaybackStateStopped;
						break;
				}
			}

			// The logo, straight out of the resources. The artwork object asks for the
			// image at whatever size it wants to draw, so one NSImage serves every size.
			void	loadArtwork()
			{
				QImage	image(QString::fromLatin1(IconResource));

				if (image.isNull())
					return;

				CGImageRef	cgImage = image.toCGImage();

				if (!cgImage)
					return;

				NSImage	*nsImage = [[NSImage alloc] initWithCGImage:cgImage
					size:NSMakeSize(image.width(), image.height())];

				CGImageRelease(cgImage);

				_artwork = [[MPMediaItemArtwork alloc] initWithBoundsSize:nsImage.size
					requestHandler:^NSImage *(CGSize size)
					{
						(void)size;
						return nsImage;
					}];

				// The block above holds its own reference now.
#if !__has_feature(objc_arc)
				[nsImage release];
#endif
			}
	};
}

MediaControls	*createPlatformMediaControls(QObject *parent)
{
	return new MacMediaControls(parent);
}

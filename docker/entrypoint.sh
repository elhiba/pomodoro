#!/usr/bin/env bash
#
# The container's front door. Turns a one-word command into the cmake invocation behind
# it, so nobody has to remember the flags -- and so `docker compose run test` and the CI
# job that checks this image agree on what "test" means.

set -euo pipefail

BUILD_DIR="${BUILD_DIR:-/work/build-docker}"

# Configure only when there is no cache yet, or when CMakeLists.txt is newer than the
# cache. Re-running cmake on every invocation would work but costs a few seconds each
# time, and the whole point of the bind mount is a fast edit-compile-test loop.
configure()
{
	if [ ! -f "$BUILD_DIR/CMakeCache.txt" ] || [ CMakeLists.txt -nt "$BUILD_DIR/CMakeCache.txt" ]; then
		cmake -S . -B "$BUILD_DIR" -G Ninja \
			-DCMAKE_BUILD_TYPE=Release \
			-DBUILD_TESTING=ON
	fi
}

compile()
{
	configure
	cmake --build "$BUILD_DIR" --parallel
}

case "${1:-test}" in
	# Compile everything, nothing else.
	build)
		compile
		;;

	# What a pull request should be judged on: it has to compile, and the timer's state
	# machine has to still behave.
	test)
		compile
		ctest --test-dir "$BUILD_DIR" --output-on-failure
		;;

	# Starts the app headless, lets it settle, and fails on anything that means the UI
	# did not really come up. `test` cannot catch this: it exercises the timer's state
	# machine and never loads a QML file or an icon, which is exactly how a missing SVG
	# image plugin reached a contributor with every title-bar button blank.
	#
	# Audio is not checked. A container has no sound device and QSoundEffect says so
	# loudly; that is expected here and must not fail the run.
	smoke)
		compile

		log=$(mktemp)
		code=0

		# 15s is long enough for the window, the QML engine and the first paint. The app
		# is meant to keep running, so being killed at the end is the success case.
		QT_QPA_PLATFORM=offscreen timeout --signal=TERM 15 "$BUILD_DIR/pomodoro" >"$log" 2>&1 || code=$?

		# 124 is timeout doing its job; 0 would mean the app quit by itself, which is
		# fine too. Anything else is a crash or a failed start.
		if [ "$code" -ne 124 ] && [ "$code" -ne 0 ]; then
			echo "smoke: the app exited with $code" >&2
			cat "$log" >&2
			exit 1
		fi

		if grep -qiE 'Unsupported image format|Error decoding|is not installed|module .* not found|QQmlApplicationEngine failed' "$log"; then
			echo "smoke: the window came up but its assets did not" >&2
			grep -iE 'Unsupported image format|Error decoding|is not installed|module .* not found|QQmlApplicationEngine failed' "$log" >&2
			exit 1
		fi

		echo "smoke: the app started and loaded its QML and icons cleanly"
		;;

	# Opens the actual window. Needs an X server on the host and DISPLAY passed through;
	# docker-compose.yml wires that up, and the README says which hosts it works on.
	run)
		compile
		exec "$BUILD_DIR/pomodoro"
		;;

	# Throw the build tree away without touching anything on the host except the
	# directory the container created.
	clean)
		rm -rf "${BUILD_DIR:?}"
		echo "removed $BUILD_DIR"
		;;

	# An interactive prompt with the whole toolchain already on PATH.
	shell)
		exec /bin/bash
		;;

	# Anything else is run verbatim, so `docker compose run test cmake --version` works.
	*)
		exec "$@"
		;;
esac

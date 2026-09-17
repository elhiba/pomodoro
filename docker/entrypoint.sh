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

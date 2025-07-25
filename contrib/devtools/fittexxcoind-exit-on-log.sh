#!/usr/bin/env bash

export LC_ALL=C

set -euxo pipefail

TOPLEVEL=$(git rev-parse --show-toplevel)
DEFAULT_FITTEXXCOIND="${TOPLEVEL}/build/src/fittexxcoind"
DEFAULT_LOG_FILE=~/".fittexxcoin/debug.log"

help_message() {
  set +x
  echo "Run fittexxcoind until a given log message is encountered, then kill fittexxcoind."
  echo ""
  echo "Example usages:"
  echo "$0 --grep 'progress=1.000000' --params \"-datadir=~/.fittexxcoin\" --callback mycallback"
  echo ""
  echo "Options:"
  echo "-h, --help            Display this help message."
  echo ""
  echo "-g, --grep            (required) The grep pattern to look for."
  echo ""
  echo "-c, --callback        (optional) Bash command to execute as a callback. This is useful for interacting with fittexxcoind before it is killed (to run tests, for example)."
  echo "-p, --params          (optional) Parameters to provide to fittexxcoind."
  echo ""
  echo "Environment Variables:"
  echo "FITTEXXCOIND              Default: ${DEFAULT_FITTEXXCOIND}"
  echo "LOG_FILE              Default: ${DEFAULT_LOG_FILE}"
  set -x
}

: "${FITTEXXCOIND:=${DEFAULT_FITTEXXCOIND}}"
: "${LOG_FILE:=${DEFAULT_LOG_FILE}}"
GREP_PATTERN=""
CALLBACK=""
FITTEXXCOIND_PARAMS=""

# Parse command line arguments
while [[ $# -gt 0 ]]; do
case $1 in
  -h|--help)
    help_message
    exit 0
    ;;

  # Required params
  -g|--grep)
    GREP_PATTERN="$2"
    shift # shift past argument
    shift # shift past value
    ;;

  # Optional params
  -c|--callback)
    CALLBACK="$2"
    shift # shift past argument
    shift # shift past value
    ;;
  -p|--params)
    FITTEXXCOIND_PARAMS="$2"
    shift # shift past argument
    shift # shift past value
    ;;

  *)
    echo "Unknown argument: $1"
    help_message
    exit 1
    ;;
esac
done

if [ -z "${GREP_PATTERN}" ]; then
  echo "Error: A grep pattern was not specified."
  echo ""
  help_message
  exit 1
fi

# Make sure the debug log exists so that tail does not fail
touch "${LOG_FILE}"

# Launch fittexxcoind using custom parameters
read -ar FITTEXXCOIND_PARAMS <<< "${FITTEXXCOIND_PARAMS}"
START_TIME=$(date +%s)
if [ "${#FITTEXXCOIND_PARAMS[@]}" -gt 0 ]; then
  "${FITTEXXCOIND}" "${FITTEXXCOIND_PARAMS[@]}" &
else
  "${FITTEXXCOIND}" &
fi
FITTEXXCOIND_PID=$!

# Wait for log checking to finish and kill the daemon
(
  # When this subshell finishes, kill fittexxcoind
  log_subshell_cleanup() {
    echo "Cleaning up fittexxcoin daemon (PID: ${FITTEXXCOIND_PID})."
    kill ${FITTEXXCOIND_PID}
  }
  trap "log_subshell_cleanup" EXIT

  (
    # Ignore the broken pipe when tail tries to write pipe closed by grep
    set +o pipefail
    tail -f "${LOG_FILE}" | grep -m 1 "${GREP_PATTERN}"
  )

  END_TIME=$(date +%s)
  RUNTIME=$((END_TIME - START_TIME))
  HUMAN_RUNTIME=$(eval "echo $(date -ud "@${RUNTIME}" "+\$((%s/3600)) hours, %M minutes, %S seconds")")

  echo "Grep pattern '${GREP_PATTERN}' found after ${HUMAN_RUNTIME}."

  # Optional callback for interacting with fittexxcoind before it's killed
  if [ -n "${CALLBACK}" ]; then
    "${CALLBACK}"
  fi
) &
LOG_PID=$!

# Wait for fittexxcoind to exit, whether it exited on its own or the log subshell finished
wait ${FITTEXXCOIND_PID}
FITTEXXCOIND_EXIT_CODE=$?

if [ "${FITTEXXCOIND_EXIT_CODE}" -ne "0" ]; then
  echo "fittexxcoind exited unexpectedly with code: ${FITTEXXCOIND_EXIT_CODE}"
  exit ${FITTEXXCOIND_EXIT_CODE}
fi

# Get the exit code for the log subshell, which should have exited already
wait ${LOG_PID}
LOG_EXIT_CODE=$?

# The log subshell should only exit with a non-zero code if the callback
# failed.
if [ "${LOG_EXIT_CODE}" -ne "0" ]; then
  echo "Log subshell failed with code: ${LOG_EXIT_CODE}"
  exit ${LOG_EXIT_CODE}
fi

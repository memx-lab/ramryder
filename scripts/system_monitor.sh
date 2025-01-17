#!/bin/bash
set -x

# PCM config
PCM_HOME="/home/yaz093/tool/pcm-extend"
PCM_MONITOR_DELAY_SECONDS="1"
PCM_OUTPUT_NAME="mlrm-1024mnb-1s.csv"

# run case config
CASE_HOME="/home/yaz093/tool/dlrm/"
EXEC_CASE="./bench/dlrm_s_criteo_kaggle.sh --test-freq=1024"

# binary
PCM_MONITOR_BIN="$PCM_HOME/build/bin/pcm-memory"
PCM_MONITOR_EXT_PAR="$PCM_MONITOR_DELAY_SECONDS -silent -partial -csv=$PCM_OUTPUT_NAME -f"
EXEC_PCM_MONITOR="sudo $PCM_MONITOR_BIN $PCM_MONITOR_EXT_PAR"


# start test
$EXEC_PCM_MONITOR &
pushd $CASE_HOME
$EXEC_CASE
sudo pkill -f $PCM_MONITOR_BIN




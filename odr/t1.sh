#!/bin/bash

SESSION=tmx1

ODR=odr_6872
SERVER=server_6872
STALE=1000

tmux new-session -d -s $SESSION

tmux split-window -h
tmux select-pane -t 0
tmux split-window -v
tmux select-pane -t 2
tmux split-window -v

tmux select-pane -t 0
tmux split-window -v

tmux select-pane -t 2
tmux split-window -v

tmux select-pane -t 4
tmux split-window -v

tmux select-pane -t 6
tmux split-window -v

for i in `seq 0 7`;
do
    tmux select-pane -t $i
    tmux send-keys "ssh root@vm$(( $i + 1))" C-m 
    sleep 5
    tmux send-keys "cd /home/tpalit/" C-m
    tmux send-keys "./$ODR $STALE &" C-m
    tmux send-keys "./$SERVER $STALE &" C-m
done    

tmux attach-session -t tmx1

# tmux select-pane -t 0
# tmux send-keys "ssh root@vm1" C-m 
# tmux send-keys "cd /home/tpalit/" C-m
# tmux send-keys "./$ODR $STALE &" C-m
# tmux send-keys "./$SERVER $STALE &" C-m
# tmux select-pane -t 1
# tmux send-keys "ssh root@vm2" C-m 
# tmux send-keys "cd /home/tpalit/" C-m
# tmux send-keys "./$ODR $STALE &" C-m
# tmux send-keys "./$SERVER $STALE &" C-m

# tmux select-pane -t 2
# tmux send-keys "ssh root@vm3" C-m 
# tmux send-keys "cd /home/tpalit/" C-m
# tmux send-keys "./$ODR $STALE &" C-m
# tmux send-keys "./$SERVER $STALE &" C-m

# tmux select-pane -t 3
# tmux send-keys "ssh root@vm4" C-m 
# tmux send-keys "cd /home/tpalit/" C-m
# tmux send-keys "./$ODR $STALE &" C-m
# tmux send-keys "./$SERVER $STALE &" C-m

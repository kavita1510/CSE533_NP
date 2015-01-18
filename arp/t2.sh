#!/bin/bash                                                                                                          

SESSION=tmx2

ARP=tpalit_arp
TOUR=tpalit_tour

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

for i in `seq 0 1`;
do
    tmux select-pane -t $i
    tmux send-keys "ssh root@vm$(( $i + 9))" C-m
    sleep 5
    tmux send-keys "cd /home/tpalit/" C-m
    tmux send-keys "./$ARP &" C-m
    tmux send-keys "./$TOUR &" C-m
done

tmux attach-session -t tmx2


rsync -avz -e ssh --progress --delete $BFS_HOME bfs-pi205:/home/$USER/repos
rsync -avz -e ssh --progress --delete $BFS_HOME bfs-pi206:/home/$USER/repos
rsync -avz -e ssh --progress --delete $BFS_HOME bfs-pi207:/home/$USER/repos
rsync -avz -e ssh --progress --delete $BFS_HOME bfs-pi208:/home/$USER/repos

rsync -avz -e ssh --progress --delete $BFS_HOME nuc:/home/$USER/repos
rsync -avz -e ssh --progress --delete $BFS_HOME nuc2:/home/$USER/repos

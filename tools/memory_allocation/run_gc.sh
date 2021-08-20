#!/bin/bash
blue=$(tput setaf 4)
normal=$(tput sgr0)
for t in 10 20 40 80
do
	printf "\n\n${blue}*** Result for $t threads ***\n${normal}"
	python3 main.py --file_name=alloc_list_withperm/graphchi/$t --init_file_name=init_map_list/graphchi
done

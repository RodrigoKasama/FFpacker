#!/bin/bash

Listname="FILELIST"
dir_path="$(pwd)/$Listname";
file_path="$(pwd)/$Listname/$Listname.txt";
timeout="0";

if [[ ! -d "$dir_path" ]]; then
	echo "Diretorio $Listname inexistente. Criando..";
	mkdir "$dir_path";
fi

if [[ ! -e "$file_path" ]]; then
	echo "Arquivo $Listname/$Listname.txt inexistente. Criando..";
	touch "$file_path";
fi


echo $@ $dir_path $file_path $timeout
./rtsp_try $@ $dir_path $file_path $timeout;

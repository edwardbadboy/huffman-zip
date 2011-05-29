#!/bin/bash

run_cmd(){
	COUNTER=$1
	CMD=$2
	while [ $COUNTER -gt 0 ]; do
		../$CMD >/dev/null 2>&1 << EOF
tags
EOF
	COUNTER=$(($COUNTER-1))
	done
	return 0
}

echo "begin to test huffman_zip_heap "$1" times..."
time run_cmd $1 "huffman_zip_heap"
diff tags tags.unhzip >/dev/null
if [ $? -eq 0 ]; then
	echo "test ok"
else
	echo "test failed"
fi
echo ""

echo "begin to test huffman_zip "$1" times..."
time run_cmd $1 "huffman_zip"
diff tags tags.unhzip >/dev/null
if [ $? -eq 0 ]; then
	echo "test ok"
else
	echo "test failed"
fi

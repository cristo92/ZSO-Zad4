#!/bin/bash
#make -s ${1};
#for input in ${1}_test/*.in;
#do
#	TIME=`(time ./${1} < $input > out) 2>&1 | awk '{ if(NR == 2) print $2 }'`;
#	diff -w out ${input:0:${#input}-3}.out
#	if [ $? -eq 0 ]
#	then
#		printf "[\e[32mOK\e[0m]\t%s\t%s\n" ${input:${#1}+6} $TIME
#	fi
#done

OK() { 
	printf "Test%d\t[\e[32mOK\e[0m]\n" $1
}
WRONG() {
	printf "Test%d\t[\e[30mWRONG\e[0m]\n" $1
}

CHECK() {
	if [ $? -eq 0 ]
	then 
		OK $1
	else
		WRONG $1
	fi
}

# Test 1
./test1
if [ $? -eq 0 ]
then
	OK 1
else
	WRONG 1
fi

# Test 2
./test2a aaaaaaaaaaaaa 1 & ./test2a bbbbbbbbbbbbbbbbb 1 & ./test2a cccccccccccc 1 & ./test2a dddddddddddddddddd 1 &
./test2b & ./test2b & ./test2b & ./test2b &
./test2a eeeeeeeeee 1 & ./test2a fffffffffff 1 & ./test2a gggggggggggggg 1 & ./test2a hhhhhhhhhhhhh 1 &
./test2b & ./test2b & ./test2b & ./test2b &
./test2a iiiiiiiiiii 1 & ./test2a jjjjjjjjjj 1 & ./test2a kkkkkkkkkkk 1 & ./test2a llllllllllllll 1 > /dev/null
if [ $? -eq 0 ]
then
	OK 2
else
	WRONG 2
fi

# Test 3
./test3
if [ $? -eq 0 ]
then
	OK 3
else
	WRONG 3
fi

# Test 5
./test5
CHECK 5

# Test 6
./test6a 64 "&&&&llllmmmm" 1 & ./test6a 16 "&&&&bbbbccccdddd" 1 & \
./test6a 32 "&&&&eeeeffffgggghhhhiiiijjjjkkkk" 1 & \
./test6a 64 "&&&&llllmmmm" 0 & ./test6a 16 "&&&&bbbbccccdddd" 0 & \
./test6a 32 "&&&&eeeeffffgggghhhhiiiijjjjkkkk" 0 & \
./test6a 64 "&&&&llllmmmm" 1 & ./test6a 16 "&&&&bbbbccccdddd" 1 & \
./test6a 32 "&&&&eeeeffffgggghhhhiiiijjjjkkkk" 1 &

./test6b 16 "&&&&bbbbccccdddd&&&&eeeeffffgggghhhhiiiijjjjkkkk&&&&llllmmmm"
CHECK 6

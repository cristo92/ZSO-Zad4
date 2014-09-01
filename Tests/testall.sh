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
# Proste wywolanie write i read
./test1
CHECK 1

# Test 2 
# Rownolegle wywolywanie write i read
./test2a aaaaaaaaaaaaa 1 & ./test2a bbbbbbbbbbbbbbbbb 1 & ./test2a cccccccccccc 1 & ./test2a dddddddddddddddddd 1 &
./test2b & ./test2b & ./test2b & ./test2b &
./test2a eeeeeeeeee 1 & ./test2a fffffffffff 1 & ./test2a gggggggggggggg 1 & ./test2a hhhhhhhhhhhhh 1 &
./test2b & ./test2b & ./test2b & ./test2b &
./test2a iiiiiiiiiii 1 & ./test2a jjjjjjjjjj 1 & ./test2a kkkkkkkkkkk 1 & ./test2a llllllllllllll 1 > /dev/null
CHECK 2

# Test 3
# Proste wywolanie pwrite i pread
./test3
CHECK 3

# Test 4
# Test na lseek
./test4
CHECK 4

# Test 5
# Proste wywolanie pwrite i pread, tak zeby transakcja przecinała wiele różnych
# bloków.
./test5
CHECK 5

# Test 6
# Test na równoległe niekonfliktujące zapisy
./test6a 64 "&&&&llllmmmm" 1 & ./test6a 16 "&&&&bbbbccccdddd" 1 & \
./test6a 32 "&&&&eeeeffffgggghhhhiiiijjjjkkkk" 1 & \
./test6a 64 "&&&&llllmmmm" 0 & ./test6a 16 "&&&&bbbbccccdddd" 0 & \
./test6a 32 "&&&&eeeeffffgggghhhhiiiijjjjkkkk" 0 & \
./test6a 64 "&&&&llllmmmm" 1 & ./test6a 16 "&&&&bbbbccccdddd" 1 & \
./test6a 32 "&&&&eeeeffffgggghhhhiiiijjjjkkkk" 1

./test6b 16 "&&&&bbbbccccdddd&&&&eeeeffffgggghhhhiiiijjjjkkkk&&&&llllmmmm"
CHECK 6

# Test 7
# Test na równoległe czytanie przez wątki tego samego procesu
./test7
CHECK 7

# Test 8
# Test na równoległe czytanie przez wiele procesów
./test6a 16 "&&&&bbbbccccdddd&&&&eeeeffffgggghhhhiiiijjjjkkkk&&&&llllmmmm" 1

./test6b 16 "&&&&bbbbccccdddd&&&&eeeeffffgggghhhhiiiijjjjkkkk&&&&llllmmmm" & \
./test6b 16 "&&&&bbbbccccdddd&&&&eeee" & \
./test6b 64 "&&&&" & \
./test6b 20 "bbbbccccdddd&&&&eeeeffffgggghhhhiiiijjjjkkkk&&&&llllmmmm" & \
./test6b 20 "bbbbccccdddd&&&&eeeeffffgggghhh" & \
./test6b 64 "&&&&" & \
./test6b 32 "&&&&eeeeffffgggghhhhiiiijjjjkkkk&&&&llllmmmm" & \
./test6b 32 "&&&&eeeeffffgggghhhhiiiijjjjk" & \
./test6b 64 "&&&&" & \
./test6b 48 "hhhhiiiijjjjkkkk&&&&llllmmmm" & \
./test6b 48 "hhhh" & \
./test6b 20 "bbbbccccdddd&&&&eeeeffffgggghhh" & \
./test6b 32 "&&&&eeeeffffgggghhhhiiiijjjjkkkk&&&&llllmmmm" & \
./test6b 16 "&&&&bbbbccccdddd&&&&eeee"  & \
./test6b 20 "bbbbccccdddd&&&&eeeeffffgggghhhhiiiijjjjkkkk&&&&llllmmmm" & \
./test6b 32 "&&&&eeeeffffgggghhhhiiiijjjjk" & \
./test6b 64 "&&&&llllmmmm" & \
./test6b 20 "bbbbccccdddd&&&&eeeeffffgggghhhhiiiijjjjkkkk&&&&llllmmmm" & \
./test6b 20 "bbbbccccdddd&&&&eeeeffffgggghhh" & \
./test6b 32 "&&&&eeeeffffgggghhhhiiiijjjjkkkk&&&&llllmmmm" & \
./test6b 64 "&&&&" & \
./test6b 16 "&&&&bbbbccccdddd&&&&eeeeffffgggghhhhiiiijjjjkkkk&&&&llllmmmm" & \
./test6b 16 "&&&&bbbbccccdddd&&&&eeeeffffgggghhhhiiiijjjjkkkk&&&&llllmmmm" & \
./test6b 16 "&&&&bbbbccccdddd&&&&eeeeffffgggghhhhiiiijjjjkkkk&&&&llllmmmm" 
CHECK 8

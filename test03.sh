#!/usr/bin/env bash
rm -f R.*


RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

make clean && make

echo -e "${RED}       红黑树课堂提供，更多测试脚本，添加瑞哥微信：marey_marey111${NC}"
echo -e "${RED}       红黑树课堂 更专业的课程辅导机构，助力冲刺满分!!!${NC}"
echo -e "${RED}       红黑树恭喜你，已经到了测试这步了，已经超越了90%的人!!!${NC}"

chmod +x ./create
chmod +x ./insert
chmod +x ./query
chmod +x ./stats

./create R 3 2 "0,0:1,0:2,0:0,1:1,1:2,1"
./insert R < data1.txt
# echo "insert data successfully\n"
#
./stats  R

echo "query '*' from R where '1042,?,?' | sort"
./query '*' from R where '1042,?,?' | sort

echo "query '*' from R where '?,horoscope,?' | sort"
./query '*' from R where '?,horoscope,?' | sort
echo "query '*' from R where '?,?,shoes' | sort"
./query '*' from R where '?,?,shoes' | sort

echo " ./query '*' from R where '?,b%,d%' | sort"
./query '*' from R where '?,b%,d%' | sort


echo " ./query '*' from R where '101%,?,?' | sort"
./query '*' from R where '101%,?,?' | sort

echo " ./query '1,2' from R where '101%,?,?' | sort'"
./query '1,2' from R where '101%,?,?' | sort

echo " ./query '2,1' from R where '101%,?,?' | sort"
./query '2,1' from R where '101%,?,?' | sort


echo " ./query '2,1' from R where '%%101%,?,?' | sort"
./query '2,1' from R where '%%101%,?,?' | sort

echo " ./query '2' from R where '101%,s%o%r%,?' | sort"
./query '2' from R where '101%,s%o%r%,?' | sort



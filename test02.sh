# 删除任何现有的数据库文件
rm -f R.*

# 重新编译项目
make clean && make

# Remove any existing relation
rm -f R.*

./create R 3 2 "0,0:1,0:2,0"

./insert	R	<	data1.txt

# Final state
./stats R

#./dump  R
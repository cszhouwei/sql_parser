sql_parser
==========

基于boost.spirit实现的简易SQL解析器，仅支持select的部分语法

time ./test sql.txt 100000 0

-O0 约1.3w/s

-O2 约20w/s

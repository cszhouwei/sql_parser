#include "sql_parser.h"
#include <fstream>

int main(int argc, char *argv[])
{
    if (argc < 2) {
        std::cerr << "usage: " << argv[0] << " file [num] [print]" << std::endl;
        return -1;
    }
    int run_num = 1;
    if (argc >= 3) {
        run_num = atoi(argv[2]);
    }
    int print = 1;
    if (argc >= 4) {
        print = atoi(argv[3]);
    }

    std::ifstream ifs(argv[1], std::ios::in);
    std::vector<std::string> sqls;
    while (!ifs.eof()) {
        char sql_buf[255];
        ifs.getline(sql_buf, sizeof(sql_buf));
        size_t sql_len = strlen(sql_buf);
        if (sql_len <= 0) continue;
        sqls.push_back(sql_buf);
    }

    SQLParser<std::string::const_iterator> sql_parser;
    for (int i = 0; i < run_num; ++i) {
        for (std::vector<std::string>::iterator iter = sqls.begin(); iter != sqls.end(); ++iter) {
            std::string::const_iterator begin = iter->begin();
            std::string::const_iterator end = iter->end();
            SelectSQL select_sql;
            bool ret = phrase_parse(begin, end, sql_parser, boost::spirit::ascii::space, select_sql);
            if (ret && begin == end) {
                if (print) {
                   std::cout << "phrase_parse succ. sql=" << *iter << std::endl;
                   for (std::vector<std::string>::iterator iter = select_sql.fields.begin(); iter != select_sql.fields.end(); ++iter) {
                   std::cout << "[field]=" << *iter << std::endl;
                   }
                   std::cout << "[table]=" << select_sql.table << std::endl;
                   if (select_sql.has_condition) {
                   std::cout << "[condition]=" << select_sql.condition << std::endl;
                   }
                   std::cout << "=============" << std::endl;
                }
            } else {
                std::cerr << "[FAIL]phrase_parse fail. sql=" << *iter << std::endl;
                std::cerr << "=============" << std::endl;
            }
        }
    }

    return 0;
}

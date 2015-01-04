#ifndef SQL_PARSER_H_
#define SQL_PARSER_H_

#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/phoenix.hpp>
#include <boost/fusion/include/adapt_struct.hpp>

struct CompareOp : boost::spirit::qi::symbols<char, unsigned>
{
    enum Type
    {
        kTypeLT   = 1,
        kTypeGT   = 2,
        kTypeLE   = 3,
        kTypeGE   = 4,
        kTypeEQ   = 5,
        kTypeNE   = 6,
    };

    CompareOp()
    {
        add
            ("<"    , kTypeLT)
            (">"    , kTypeGT)
            ("<="   , kTypeLE)
            (">="   , kTypeGE)
            ("="    , kTypeEQ)
            ("!="   , kTypeNE)
        ;
    }

} compare_op;

struct Literal
{
    enum Type
    {
        kTypeNone   = 0,
        kTypeBool   = 1,
        kTypeLong   = 2,
        kTypeDouble = 3,
        kTypeString = 4,
    };
    typedef boost::variant<bool, long, double, std::string> Value;

    Type type;
    Value value;
};
BOOST_FUSION_ADAPT_STRUCT(
    Literal,
    (Literal::Type, type)
    (Literal::Value, value)
)
std::ostream & operator << (std::ostream &os, const Literal &literal)
{
    return os << "<Literal type=" << (int)literal.type << " value=" << literal.value << " />";
}

struct Compare
{
    std::string left;
    CompareOp::Type op;
    Literal right;
};
BOOST_FUSION_ADAPT_STRUCT(
    Compare,
    (std::string, left)
    (CompareOp::Type, op)
    (Literal, right)
)
std::ostream & operator << (std::ostream &os, const Compare &compare)
{
    return os << "<Compare left=" << compare.left << " op=" << (int)compare.op << " right=" << compare.right << " />";
}

struct Condition
{
    enum OpType
    {
        kOpTypeNONE   = 0,
        kOpTypeAND    = 1,
        kOpTypeOR     = 2,
    };
    typedef boost::variant<Compare, boost::recursive_wrapper<Condition> > NodeType;

    NodeType left;
    OpType op;
    NodeType right;

    Condition() : op(kOpTypeNONE) {}
};
BOOST_FUSION_ADAPT_STRUCT(
    Condition,
    (Condition::NodeType, left)
    (Condition::OpType, op)
    (Condition::NodeType, right)
)
std::ostream & operator << (std::ostream &os, const Condition &condition)
{
    if (condition.op == Condition::kOpTypeNONE) {
        return os << condition.left;
    } else {
        return os << "<Condition left=" << condition.left << " op=" << (int)condition.op << " right=" << condition.right << " />";
    }
}

struct SelectSQL
{
    std::vector<std::string> fields;
    std::string table;
    bool has_condition;
    Condition condition;

    SelectSQL() : has_condition(false) {}
};
BOOST_FUSION_ADAPT_STRUCT(
    SelectSQL,
    (std::vector<std::string>, fields)
    (std::string, table)
    (bool, has_condition)
    (Condition, condition)
)

struct StringLiteral
{
    template <typename T1, typename T2 = void>
    struct result { typedef void type; };

    void operator() (Literal &literal, char ch) const
    {
        boost::get<std::string>(literal.value).push_back(ch);
    }
};
static const boost::phoenix::function<StringLiteral> string_literal_func;

template <typename Iterator>
struct SQLParser : boost::spirit::qi::grammar<Iterator, SelectSQL(), boost::spirit::ascii::space_type>
{
    SQLParser() : SQLParser::base_type(select)
    {
        identifier %=
            boost::spirit::qi::lexeme[
                boost::spirit::ascii::alpha
                > *(boost::spirit::ascii::alnum | boost::spirit::ascii::char_('_'))
            ]
            ;

        bool_literal =
            boost::spirit::qi::bool_    [boost::phoenix::at_c<0>(boost::spirit::qi::_val) = Literal::kTypeBool, boost::phoenix::at_c<1>(boost::spirit::qi::_val) = boost::spirit::qi::_1]
            ;

        number_literal =
            boost::spirit::qi::real_parser<double, boost::spirit::qi::strict_real_policies<double> >()  [boost::phoenix::at_c<0>(boost::spirit::qi::_val) = Literal::kTypeDouble, boost::phoenix::at_c<1>(boost::spirit::qi::_val) = boost::spirit::qi::_1]
            | boost::spirit::qi::long_                                                                  [boost::phoenix::at_c<0>(boost::spirit::qi::_val) = Literal::kTypeLong, boost::phoenix::at_c<1>(boost::spirit::qi::_val) = boost::spirit::qi::_1]
            ;

        string_literal =
            boost::spirit::qi::lexeme[
                (boost::spirit::ascii::char_('"')                                                       [boost::spirit::qi::_a = boost::spirit::qi::_1]
                 | boost::spirit::ascii::char_('\'')                                                    [boost::spirit::qi::_a = boost::spirit::qi::_1]
                 )                                                                                      [boost::phoenix::at_c<0>(boost::spirit::qi::_val) = Literal::kTypeString, boost::phoenix::at_c<1>(boost::spirit::qi::_val) = std::string("")]
                > *(boost::spirit::ascii::char_ - boost::spirit::ascii::char_(boost::spirit::qi::_a))   [string_literal_func(boost::spirit::qi::_val, boost::spirit::qi::_1)]
                > boost::spirit::ascii::char_(boost::spirit::qi::_a)
            ];

        literal %=
            string_literal
            | bool_literal
            | number_literal
            ;

        table = identifier;

        fields %=
            identifier % ','
            ;

        compare %=
            identifier
            > compare_op
            > literal
            ;

        condition =
            condition_high                              [boost::spirit::qi::_val = boost::spirit::qi::_1]
            > *(boost::spirit::ascii::no_case["or"]
                    > condition_high                    [boost::phoenix::at_c<0>(boost::spirit::qi::_a) = boost::spirit::qi::_val, boost::phoenix::at_c<1>(boost::spirit::qi::_a) = Condition::kOpTypeOR, boost::phoenix::at_c<2>(boost::spirit::qi::_a) = boost::spirit::qi::_1, boost::spirit::qi::_val = boost::spirit::qi::_a]
                    )
            ;

        condition_high =
            condition_atom                              [boost::spirit::qi::_val = boost::spirit::qi::_1]
            > *(boost::spirit::ascii::no_case["and"]
                    > condition_atom                    [boost::phoenix::at_c<0>(boost::spirit::qi::_a) = boost::spirit::qi::_val, boost::phoenix::at_c<1>(boost::spirit::qi::_a) = Condition::kOpTypeAND, boost::phoenix::at_c<2>(boost::spirit::qi::_a) = boost::spirit::qi::_1, boost::spirit::qi::_val = boost::spirit::qi::_a]
                    )
            ;

        condition_atom =
            compare             [boost::phoenix::at_c<0>(boost::spirit::qi::_val) = boost::spirit::qi::_1]
            | ('('> condition   [boost::spirit::qi::_val = boost::spirit::qi::_1] > ')')
            ;

        select =
            boost::spirit::ascii::no_case["select"]
            > fields                                        [boost::phoenix::at_c<0>(boost::spirit::qi::_val) = boost::spirit::qi::_1]
            > boost::spirit::ascii::no_case["from"]
            > table                                         [boost::phoenix::at_c<1>(boost::spirit::qi::_val) = boost::spirit::qi::_1]
            > -(boost::spirit::ascii::no_case["where"]      [boost::phoenix::at_c<2>(boost::spirit::qi::_val) = true]
                    > condition                             [boost::phoenix::at_c<3>(boost::spirit::qi::_val) = boost::spirit::qi::_1]
                    )
            ;

        boost::spirit::qi::on_error<boost::spirit::qi::fail>
        (
         select,
         std::cout
         << boost::phoenix::val("Error! Expecting ")
         << boost::spirit::qi::_4
         << boost::phoenix::val(" here: \"")
         << boost::phoenix::construct<std::string>(boost::spirit::qi::_3, boost::spirit::qi::_2)
         << boost::phoenix::val("\"")
         << std::endl
        );
    }

    boost::spirit::qi::rule<Iterator, std::string(), boost::spirit::ascii::space_type> identifier;
    boost::spirit::qi::rule<Iterator, Literal(), boost::spirit::ascii::space_type> bool_literal;
    boost::spirit::qi::rule<Iterator, Literal(), boost::spirit::ascii::space_type> number_literal;
    boost::spirit::qi::rule<Iterator, Literal(), boost::spirit::qi::locals<char>, boost::spirit::ascii::space_type> string_literal;
    boost::spirit::qi::rule<Iterator, Literal(), boost::spirit::ascii::space_type> literal;
    boost::spirit::qi::rule<Iterator, std::string(), boost::spirit::ascii::space_type> table;
    boost::spirit::qi::rule<Iterator, std::vector<std::string>(), boost::spirit::ascii::space_type> fields;
    boost::spirit::qi::rule<Iterator, Compare(), boost::spirit::ascii::space_type> compare;
    boost::spirit::qi::rule<Iterator, Condition(), boost::spirit::qi::locals<Condition>, boost::spirit::ascii::space_type> condition;
    boost::spirit::qi::rule<Iterator, Condition(), boost::spirit::qi::locals<Condition>, boost::spirit::ascii::space_type> condition_high;
    boost::spirit::qi::rule<Iterator, Condition(), boost::spirit::ascii::space_type> condition_atom;
    boost::spirit::qi::rule<Iterator, SelectSQL(), boost::spirit::ascii::space_type> select;
};

#endif

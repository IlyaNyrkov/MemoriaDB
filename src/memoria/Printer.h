//
// Created by Ilya Nyrkov on 20.08.25.
//

#ifndef PRINTER_H
#define PRINTER_H

#include <memoria/Table.h>
#include <iostream>
#include "memoria/StatementExecutor.h"

namespace memoria {
    class Printer {
    public:
        explicit Printer(std::ostream& out = std::cout, std::ostream& err = std::cerr)
          : out_(out), err_(err) {}

        void printQueryResult(const QueryResult& qr);     // to out_
        void printError(const std::exception& e);         // to err_
        void printHelpMessage(std::string_view prog = "memoriadb");

        // optional
        void printAffected(std::size_t n);

    private:
        std::ostream& out_;
        std::ostream& err_;

        static std::string cellToString(const RowValue& v);
        void printTable(const std::vector<std::string>& header,
                        const std::vector<Row>& rows);    // internal used by printQueryResult
    };

}

#endif //PRINTER_H

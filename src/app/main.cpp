#include "memoria/Database.h"
#include "memoria/Parser.h"
#include "memoria/Printer.h"
#include "memoria/StatementExecutor.h"
#include "memoria/StatementReader.h"

int main() {
    using namespace memoria;

    Database db;
    StatementExecutor exec{db};
    Parser parser;

    StatementReader reader{std::cin};
    Printer printer{std::cout, std::cerr};

    std::cout << "memoriadb started" << std::endl;

    while (true) {

        if (reader.readsFromCin())
            reader.printPrompt();

        auto stmtTextOpt = reader.next();
        if (!stmtTextOpt)
            break; // EOF

        const std::string& stmtText = *stmtTextOpt;
        if (stmtText.empty())
            continue;

        try {
            auto st = parser.prepareStatement(stmtText);

            auto result = exec.execute(st);
            if (result)
                printer.printQueryResult(*result);
        } catch (const std::exception& e) {
            printer.printError(e);
        }
    }
    return 0;
}

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <map>
#include <tuple>
#include <queue>
#include <regex>
#include <cstring>
#include <cstdlib>
 
using namespace std;
 
typedef int             LineNumber;
typedef vector<string>  Tokens;
typedef string          Instruction;
typedef string          Directive;
typedef int             OperandsQty;
typedef int             OpCode;
typedef int             SizeInCode;
typedef string          Label;
typedef int             MemPos;
typedef queue<string>   Errors;
 
typedef map <LineNumber, Tokens>                                    CodeLines;
typedef map <Instruction, tuple<OperandsQty, OpCode, SizeInCode> >  Instructions;
typedef map <Directive, tuple<OperandsQty, SizeInCode> >            Directives;
typedef map <Label, MemPos>                                         Labels;
typedef map <Label, string>                                         Defines;

Errors          errors;
CodeLines       codeLines;
Instructions    instructions;
Directives      directives;
Labels          labels;
Defines         defines;
string          output;

bool strReplace(std::string& str, const std::string& from, const std::string& to) {
    size_t start_pos = str.find(from);
    if(start_pos == std::string::npos)
        return false;
    str.replace(start_pos, from.length(), to);
    return true;
}

// Adiciona as instrucoes validas a estrutura do tipo Instructions
// Cada instrucao possui: Qtd. Operandos, OpCode, Tamanho
void declareInstructions () {
     
    instructions["ADD"]         = make_tuple(1,  1, 2);
    instructions["SUB"]         = make_tuple(1,  2, 2);
    instructions["MULT"]        = make_tuple(1,  3, 2);
    instructions["DIV"]         = make_tuple(1,  4, 2);
    instructions["JMP"]         = make_tuple(1,  5, 2);
    instructions["JMPN"]        = make_tuple(1,  6, 2);
    instructions["JMPP"]        = make_tuple(1,  7, 2);
    instructions["JMPZ"]        = make_tuple(1,  8, 2);
    instructions["COPY"]        = make_tuple(2,  9, 3);
    instructions["LOAD"]        = make_tuple(1, 10, 2);
    instructions["STORE"]       = make_tuple(1, 11, 2);
    instructions["INPUT"]       = make_tuple(1, 12, 2);
    instructions["OUTPUT"]      = make_tuple(1, 13, 2);
    instructions["STOP"]        = make_tuple(0, 14, 1);
}

// Adiciona as diretivas validas a estrutura do tipo Directives
// Cada diretiva possui: Qtd. Operandos, Tamanho
void declareDirectives () {

    directives["SECTION"]   = make_tuple(1, 0);
    directives["SPACE"]     = make_tuple(1, 1);
    directives["CONST"]     = make_tuple(1, 1);
    directives["BEGIN"]     = make_tuple(0, 0);
    directives["END"]       = make_tuple(0, 0);
    directives["EQU"]       = make_tuple(1, 0);
    directives["IF"]        = make_tuple(1, 0); 
}

// Pre-Processamento:
// Remove os comentarios e linhas em branco
// E coloca o codigo numa estrutura do tipo CodeLines
// Tambem verifica os labels e equs e ifs
void readAndPreProcess (const char* fileName) {
 
    ifstream infile(fileName);
    string line;
    int memPos = 0;
     
    // Le linha a linha
	for (int lineCount = 1; getline(infile, line); ++lineCount) {

        // Troca virgulas por espaco
        strReplace(line, ",", " ");

        // Ignora linhas em branco
        if (line.empty()) continue;
 
        istringstream iss(line);
        string tempStr;

        // Pega palavra a palavra de acordo com os espacos 
        while (iss >> tempStr) {
            
            // Ignora comentarios
            if (";" == tempStr.substr(0,1)) break;
 
            // Desconsidera o caso (maiusculas/minusculas)
            transform(tempStr.begin(), tempStr.end(), tempStr.begin(), ::toupper);

            // Ve se eh um label
            if (":" == tempStr.substr(tempStr.length() - 1, 1)) {

                // Remove o ':'
                tempStr = tempStr.substr(0, tempStr.length() - 1);

                string tempStr2;
                iss >> tempStr2;

                // Ve se o proximo token eh EQU
                if ("EQU" == tempStr2) {

                    string tempStr3;
                    iss >> tempStr3;

                    // Coloca o valor do EQU na tabela de defines
                    defines[tempStr] = tempStr3;

                // Senao eh label
                } else {

                    // Coloca o label na tabela de labels 
                    labels[tempStr] = memPos;

                    // Adiciona o proximo token ao vetor
                    codeLines[lineCount].push_back(tempStr2);
                }

                // Incrementa o contador da posicao de memoria
                // de acordo com o tamanho da instrucao/diretiva
                if (instructions.find(tempStr2) != instructions.end())
                    memPos += get<2>(instructions[tempStr2]);
                else if (directives.find(tempStr2) != directives.end())
                    memPos += get<1>(directives[tempStr2]);

            // Achou um define pro token
            } else if (defines.find(tempStr) != defines.end()) {

                // Adiciona o valor do define ao vetor
                codeLines[lineCount].push_back(defines[tempStr]);

            } else {

                // Adiciona os tokens ao vetor
                codeLines[lineCount].push_back(tempStr);

                // Incrementa o contador da posicao de memoria
                // de acordo com o tamanho da instrucao/diretiva
                if (instructions.find(tempStr) != instructions.end()) {
                    memPos += get<2>(instructions[tempStr]);
                } else if (directives.find(tempStr) != directives.end()) {
                    memPos += get<1>(directives[tempStr]);
                }
            }
        }
    }
}

// Compilacao em si. Gera o codigo objeto.
void compile () {
 
    stringstream tempSS;

    int memPos = 0;
 
    // Le o codigo linha a linha
    for (CodeLines::iterator codeLine = codeLines.begin(); codeLine != codeLines.end(); ++codeLine) {
         
        // Achou uma instrucao
        if (instructions.find(codeLine->second.front()) != instructions.end()) {
 
            // Escreve o OpCode da instrução
            tempSS << get<1>(instructions[codeLine->second.front()]);
            output.append(tempSS.str());
            output.append(" ");
            tempSS.str("");

            memPos += get<2>(instructions[codeLine->second.front()]);

            // O numero de operandos esta incorreto
            if (codeLine->second.size() - 1 != get<0>(instructions[codeLine->second.front()])){
                tempSS << codeLine->first;
                errors.push("ERRO NA LINHA " + tempSS.str() + ": O Numero de operandos da instrucao "+codeLine->second.front() + " esta incorreto.");
                tempSS.str("");
                continue;
            }
 
            // Procura os argumentos da instrucao na tabela de labels
            for (Tokens::iterator token = codeLine->second.begin() + 1; token != codeLine->second.end(); ++token) {

                // Achou um label na tabela
                if (labels.find(*token) != labels.end()) {
                    
                    // Escreve o endereco do label
                    tempSS << labels[*token];
                    output.append(tempSS.str());
                    output.append(" ");
                    tempSS.str("");

                // Achou um define na tabela
                } else if (defines.find(*token) != defines.end()) {

                    // Escreve o valor do define
                    tempSS << defines[*token];
                    output.append(tempSS.str());
                    output.append(" ");
                    tempSS.str("");

                // Label Inexistente
                } else {
                    tempSS << codeLine->first;
                    errors.push("ERRO NA LINHA " + tempSS.str() + ": O Label "+ *token + " nao existe.");
                }
            }
 
        // Achou uma Diretiva
        } else if (directives.find(codeLine->second.front()) != directives.end()) {
            
            memPos += get<1>(directives[codeLine->second.front()]);

            if ("SPACE" == codeLine->second.front()) {
                
                // Ve se eh um vetor
                if (codeLine->second.size() == 2) {
                    int tempNum = (int)strtol((codeLine->second[1]).c_str(), NULL, 0);

                    // Reserva um endereco ate o tamanho do vetor
                    for(int i = 0; i < tempNum; ++i)
                        output.append("0 ");

                // Se nao reserva so um endereco
                } else {
                    output.append("0 ");    
                }
                

            } else if ("CONST" == codeLine->second.front()) {
            
                int tempNum = (int)strtol((codeLine->second[1]).c_str(), NULL, 0);
                tempSS << tempNum;
                output.append(tempSS.str());
                output.append(" ");
                tempSS.str("");
            
            } else if ("IF" == codeLine->second.front()) {

                if ("0" == codeLine->second[1]) {

                    cout << endl << "IF DEU FALSE" << endl;
                    ++codeLine;

                } else {

                    cout << endl << "IF DEU TRUE" << endl;

                }
            }

        // Diretiva/Instrucao nao existe        
        } else {
            tempSS << codeLine->first;
            errors.push("ERRO NA LINHA " + tempSS.str() + ": A Diretiva/Instrucao "+codeLine->second.front() + " nao existe.");
            tempSS.str("");
        }
    }
}
 
int main(int argc, char const *argv[]) {

    char *file1 = (char*) malloc(sizeof(char)*strlen(argv[1]) + 1);
    strcpy(file1, argv[1]);
    strcat(file1, ".asm");
    
    declareInstructions();
    declareDirectives();
    readAndPreProcess(file1);
    
    cout << endl << "# LABELS #" << endl;
    for (Labels::iterator i = labels.begin(); i != labels.end(); ++i) {
        cout << i->first << ": " << i->second << endl;
    }

    cout << endl << "# DEFINES #" << endl;
    for (Defines::iterator i = defines.begin(); i != defines.end(); ++i) {
        cout << i->first << ": " << i->second << endl;
    }

    cout << endl << "# CODIGO PRE PROCESSADO #" << endl;
    for (CodeLines::iterator i = codeLines.begin(); i != codeLines.end(); ++i) {
        for (Tokens::iterator j = i->second.begin(); j != i->second.end(); ++j) {
            cout << *j << " ";
        }
        cout << endl;
    }

    compile();

    // Sem erros de traducao
    // Escreve o arquivo de saida
    if (errors.empty()) {

        char *file2 = (char*) malloc(sizeof(char)*strlen(argv[1]) + 1);
        strcpy(file2, argv[1]);
        strcat(file2, ".o");
        ofstream outputFile;
        outputFile.open(file2);
        outputFile << output;
        outputFile.close();

    // Ocorreram erros
    // Mostra e nao gera saida
    } else {

        while (!errors.empty()) {
            cout << errors.front() << endl;
            errors.pop();
        }

    }
 
    return 0;
}
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
typedef map <Label, int>                                            Defines;

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

bool validToken (string token) {
    return regex_match(token.c_str(), regex("[A-Za-z0-9_]+", regex_constants::basic));
}

// Pre-Processamento:
// Remove os comentarios e linhas em branco
// E coloca o codigo numa estrutura do tipo CodeLines
void readAndPreProcess (const char* fileName) {
 
    ifstream infile(fileName);
    string line;
     
    // Le linha a linha
	for (int lineCount = 1; getline(infile, line); ++lineCount) {
        strReplace(line, ",", " ");

        // Ignora linhas em branco
        if (line.empty()) continue;
 
        istringstream iss(line);
         
        string tempStr;
         
        while (iss >> tempStr) {
            // Ignora comentarios
            if (";" == tempStr.substr(0,1)) break;
 
            // Desconsidera o caso (maiusculas/minusculas)
            transform(tempStr.begin(), tempStr.end(), tempStr.begin(), ::toupper);          
             
            // Adiciona os tokens ao vetor
            codeLines[lineCount].push_back(tempStr);
        }
    }
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

// Primeira Passagem:
// Procura todos os labels e adiciona eles a estrutura do tipo Labels
// com os elementos: Nome do Label, Posicao em Memoria
void getLabels () {
 
    int memPos = 0; // Posição da memória onde estará o token
 
    for (CodeLines::iterator it = codeLines.begin(); it != codeLines.end(); ++it) {
 
        // Se achou ':' no fim do primeiro token da linha
        if (":" == it->second.front().substr(it->second.front().length() - 1, 1)) {
 
            //cout << it->second.front() << endl;

            // Tira o ':'
            string tempLabel = it->second.front().substr(0, it->second.front().length() - 1);
            
            

                
            //if("EQU" == it->second.begin() + 1){
                //defines[it->second.begin()] = it->second.begin() + 2;
            //}
            //else{
                // Adiciona na tabela de Labels
                labels[tempLabel] = memPos;
            //}

            // Apaga o Label do código
            it->second.erase(it->second.begin());
        }
 
        // Incrementa o contador de posição de memória
        // de acordo com a instrucao/diretiva usada
        if (instructions.find(it->second.front()) != instructions.end())
            memPos += get<2>(instructions[it->second.front()]);
        else if (directives.find(it->second.front()) != directives.end())
            memPos += get<1>(directives[it->second.front()]);
    } 
}

// Segunda Passagem:
// Compilacao em si. Gera o codigo objeto.
void compile () {
 
    //string output;
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
                errors.push("ERRO SINTATICO NA LINHA " + tempSS.str() + ": O Numero de operandos da instrucao "+codeLine->second.front() + " esta incorreto.");
                tempSS.str("");
                continue;
            }
 
            // Procura os argumentos da instrucao na tabela de labels
            for (Tokens::iterator token = codeLine->second.begin() + 1; token != codeLine->second.end(); ++token) {
         

                *token = token->substr(0,token->find("+"));


                // Achou um label na tabela
                if (labels.find(*token) != labels.end()) {
                    
                    // Escreve o endereco do label
                    tempSS << labels[*token];
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
                
                if(codeLine->second.size() == 2) {
                    int tempNum = (int)strtol((codeLine->second[1]).c_str(), NULL, 0);

                    for(int i = 0; i < tempNum; ++i){
                        output.append("0 ");
                    }

                } else{
                    output.append("0 ");    
                }
                

            } else if ("CONST" == codeLine->second.front()) {
                int tempNum = (int)strtol((codeLine->second[1]).c_str(), NULL, 0);
                tempSS << tempNum;
                output.append(tempSS.str());
                output.append(" ");
                tempSS.str("");
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
        
    readAndPreProcess(file1);
    declareInstructions();
    declareDirectives();
    getLabels();
    compile();

    for (Labels::iterator i = labels.begin(); i != labels.end(); ++i)
    {
        //cout << i->second << endl;
    }
 
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
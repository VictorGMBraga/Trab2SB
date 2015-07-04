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
typedef string          OpCode;
typedef string          CodMaquina;
typedef int             SizeInCode;
typedef string          Label;
typedef int             MemPos;
typedef queue<string>   Errors;

typedef map < LineNumber, Tokens >                                                  CodeLines;
typedef map < Instruction, tuple<OperandsQty, OpCode, CodMaquina, SizeInCode> >     Instructions;
typedef map < Directive, tuple<OperandsQty, OperandsQty, SizeInCode > >             Directives;
typedef map < Label, MemPos >                                                       Labels;
typedef map < Label, string >                                                       Defines;

enum CodeSection { NONE, TEXT, DATA };

Errors          errors;
CodeLines       codeLines;
Instructions    instructions;
Directives      directives;
Labels          labels;
Defines         defines;
string          outputText, outputData, outputBSS;
string          codOutputText, codOutputData, codOutputBSS;

const int textStartAddress = 0x08048080;
int dataStartAddress;

bool achouInput = false;
bool achouOutput = false;

bool strReplace(std::string& str, const std::string& from, const std::string& to) {
    size_t start_pos = str.find(from);
    if(start_pos == std::string::npos)
        return false;
    str.replace(start_pos, from.length(), to);
    return true;
}

// Adiciona as instrucoes validas a estrutura do tipo Instructions
// Cada instrucao possui: Qtd. Operandos, Instrucao IA-32, OpCode IA-32, Tamanho
void declareInstructions () {

    instructions["ADD"]         = make_tuple(1, "\n\tadd eax, [<ARG1>]",                            "03 05 <ADDR1>",                           6);
    instructions["SUB"]         = make_tuple(1, "\n\tsub eax, [<ARG1>]",                            "2B 05 <ADDR1>",                           6);
    instructions["MULT"]        = make_tuple(1, "\n\timul eax, [<ARG1>]",                           "0F AF <ADDR1>",                        7);
    instructions["DIV"]         = make_tuple(1, "\n\tmov ebx, [<ARG1>]\n\tidiv ebx",                "8B 1D <ADDR1>\nF7 FB",                  8);
    instructions["JMP"]         = make_tuple(1, "\n\tjmp <ARG1>",                                   "EB <RELADDR>",                         2);
    instructions["JMPN"]        = make_tuple(1, "\n\tcmp eax, 0\n\tjl <ARG1>",                      "83 F8 00\n7C <RELADDR>",                5);
    instructions["JMPP"]        = make_tuple(1, "\n\tcmp eax, 0\n\tjg <ARG1>",                      "83 F8 00\n7F <RELADDR>",                5);
    instructions["JMPZ"]        = make_tuple(1, "\n\tcmp eax, 0\n\tje <ARG1>",                      "83 F8 00\n74 <RELADDR>",                5);
    instructions["COPY"]        = make_tuple(2, "\n\tmov eax, [<ARG1>]\n\tmov [<ARG2>], eax",       "A1 <ADDR1>\nA3 <ADDR2>",               10);
    instructions["LOAD"]        = make_tuple(1, "\n\tmov eax, [<ARG1>]",                            "A1 <ADDR1>",                           5);
    instructions["STORE"]       = make_tuple(1, "\n\tmov [<ARG1>], eax",                            "A3 <ADDR1>",                           5);
    instructions["INPUT"]       = make_tuple(1, "\n\tcall LerInteiro\n\tmov DWORD [<ARG1>], eax",   " ",                                    2); // ver
    instructions["OUTPUT"]      = make_tuple(1, "\n\tmov eax, [<ARG1>]\n\tcall EscreverInteiro",    "B8 06",                                2); // ver
    instructions["STOP"]        = make_tuple(0, "\n\tmov eax, 1\n\tmov ebx, 0\n\tint 80h",          "B8 01 00 00 00\nBB 00 00 00 00\nCD 80", 12);

}

// Adiciona as diretivas validas a estrutura do tipo Directives
// Cada diretiva possui: Min. Operandos, Max. Operandos, Tamanho
void declareDirectives () {

    directives["SECTION"]   = make_tuple(1, 1, 0);
    directives["SPACE"]     = make_tuple(0, 1, 4);
    directives["CONST"]     = make_tuple(1, 1, 4);
    directives["BEGIN"]     = make_tuple(0, 1, 0);
    directives["END"]       = make_tuple(0, 0, 0);
    directives["EQU"]       = make_tuple(1, 1, 0);
    directives["IF"]        = make_tuple(1, 1, 0);

}

// Pre-Processamento:
// Remove os comentarios e linhas em branco
// E coloca o codigo numa estrutura do tipo CodeLines
// Tambem verifica os labels e equs e ifs
void readAndPreProcess (const char* fileName) {

    ifstream infile(fileName);
    string line;
    int textMemAddr = textStartAddress;
    int dataMemAddr = 0;
    int BSSMemAddr = 0;
    stringstream tempSS;

    // Le linha a linha
	for (int lineCount = 1; getline(infile, line); ++lineCount) {

        // Troca virgulas por espaco
        strReplace(line, ",", " ");

        // Ignora linhas em branco
        if (line.empty()) continue;

        // Pega palavra a palavra de acordo com os espacos
        istringstream iss(line);
        string tempStr;
        while (iss >> tempStr) {

            // Ignora comentarios
            if (";" == tempStr.substr(0,1)) break;

            // Desconsidera o caso (maiusculas/minusculas)
            transform(tempStr.begin(), tempStr.end(), tempStr.begin(), ::toupper);

            // Ve se eh um label / define
            if (":" == tempStr.substr(tempStr.length() - 1, 1)) {

                // Ve se ainda restam tokens na linha
                if (iss.rdbuf()->in_avail() != 0) {

                    // Remove o ':'
                    tempStr = tempStr.substr(0, tempStr.length() - 1);

                    string tempStr2;
                    iss >> tempStr2;

                    // Ve se o proximo token eh EQU
                    if ("EQU" == tempStr2) {

                        string tempStr3;
                        iss >> tempStr3;

                        // Se define já existe
                        if (defines.find(tempStr3) != defines.end()){
                            tempSS << lineCount;
                            errors.push("ERRO NA LINHA " + tempSS.str() + ": EQU ja declarado");
                            tempSS.str("");
                        } else {
                            // Coloca o valor do EQU na tabela de defines
                            defines[tempStr] = tempStr3;
                        }

                    // Se nao eh so um label
                    // Com algo a mais na mesma linha
                    } else {

                        if (labels.find(tempStr) != labels.end()){
                            tempSS << lineCount;
                            errors.push("ERRO NA LINHA " + tempSS.str() + ": Label ja declarado");
                            tempSS.str("");
                        } else {
                            // Adiciona na tabela de labels
                            labels[tempStr] = textMemAddr;
                        }
                        
                        // Adiciona endereco de memoria
                        if (instructions.find(tempStr2) != instructions.end())
                            textMemAddr += get<3>(instructions[tempStr2]);

                        // Adiciona os tokens ao vetor
                        codeLines[lineCount].push_back(tempStr+":");
                        codeLines[lineCount].push_back(tempStr2);

                    }

                // Se nao eh um label "sozinho"
                // Adiciona no vator
                } else {

                    if (labels.find(tempStr) != labels.end()){
                        tempSS << lineCount;
                        errors.push("ERRO NA LINHA " + tempSS.str() + ": Label ja declarado");
                        tempSS.str("");
                    } else {
                        // Adiciona na tabela de labels
                        labels[tempStr] = textMemAddr;
                    }

                    codeLines[lineCount].push_back(tempStr+":");
                
                }

            // Ve se possui define declarado
            } else if (defines.find(tempStr) != defines.end()) {

                // Adiciona o valor do define ao vetor
                codeLines[lineCount].push_back(defines[tempStr]);

            // Se nao so coloca no vetor de tokens
            } else {

                // Adiciona o token ao vetor
                codeLines[lineCount].push_back(tempStr);

            }

            // Adiciona endereco de memoria
            if (instructions.find(tempStr) != instructions.end())
                textMemAddr += get<3>(instructions[tempStr]);

        } // END WHILE que pega palavra a palavra de acordo com os espacos

    } // END FOR que le linha a linha

    dataStartAddress = textMemAddr;

}

// Compilacao em si. Gera o codigo objeto.
void compile () {

    bool achouSectionText = false;
    bool achouSectionData = false;
    CodeSection codeSection = NONE;
    stringstream tempSS;
    int memPos = 0;
    
    // Le o codigo linha a linha
    for (CodeLines::iterator codeLine = codeLines.begin(); codeLine != codeLines.end(); ++codeLine) {

        // Achou uma instrucao
        if (instructions.find(codeLine->second.front()) != instructions.end()) {

            // Se existe input, adiciona a funcao LerInteiro
            if ("INPUT" == codeLine->second[0]) {
                achouInput = true;
            }

            // Se existe output, adiciona a funcao EscreverInteiro
            if ("OUTPUT" == codeLine->second[0]) {
                achouOutput = true;
            }

            // Erro: Instrucao na secao errada
            if (TEXT != codeSection) {
                tempSS << codeLine->first;
                errors.push("ERRO NA LINHA " + tempSS.str() + ": A instrucao esta na secao errada.");
                tempSS.str("");
            }

            // O numero de operandos esta incorreto
            if (codeLine->second.size() - 1 != get<0>(instructions[codeLine->second.front()])){
                tempSS << codeLine->first;
                errors.push("ERRO NA LINHA " + tempSS.str() + ": O Numero de operandos da instrucao "+codeLine->second.front() + " esta incorreto.");
                tempSS.str("");
                continue;
            }

            if ("STOP" == codeLine->second.front()) {
                outputText.append(get<1>(instructions[codeLine->second.front()]));
                codOutputText.append(get<2>(instructions[codeLine->second.front()]));
            }

            // Procura os argumentos da instrucao na tabela de labels
            for (Tokens::iterator token = codeLine->second.begin() + 1; token != codeLine->second.end(); ++token) {

                // Achou um label na tabela
                if (labels.find(*token) != labels.end()) {
                    
                    string auxStr = get<1>(instructions[codeLine->second.front()]);
                    strReplace(auxStr, "<ARG1>", *token);

                    string auxStr2 = get<2>(instructions[codeLine->second.front()]);
                    tempSS << hex << labels[*token] + dataStartAddress;
                    strReplace(auxStr2, "<ADDR1>", tempSS.str());
                    tempSS.str("");

                    if ("COPY" == codeLine->second.front()) {
                        
                        ++token;
                        strReplace(auxStr, "<ARG2>", *token);
                        cout << auxStr;
                        outputText.append(auxStr);
                        
                        tempSS << hex << labels[*token] + dataStartAddress;
                        strReplace(auxStr2, "<ADDR2>", tempSS.str());
                        tempSS.str("");
                        codOutputText.append(auxStr2+" \n");

                        break;

                    } else {

                        cout << auxStr;
                        outputText.append(auxStr);

                        codOutputText.append(auxStr2+" \n");

                    }

                    outputText.append("\n");

                } else {
                    tempSS << dec << codeLine->first;
                    errors.push("ERRO NA LINHA " + tempSS.str() + ": O Label "+ *token + " nao existe.");
                    tempSS.str("");
                }
            }

            memPos += get<3>(instructions[codeLine->second.front()]);

        // Achou uma Diretiva
        } else if (directives.find(codeLine->second.front()) != directives.end()) {

            // Adiciona o tamanho da diretiva ao contador de posicao de memoria
            memPos += get<2>(directives[codeLine->second.front()]);

            if ("SECTION" == codeLine->second.front()) {

                if ("TEXT" == codeLine->second[1]) {
                    if (achouSectionText){
                        tempSS << dec << codeLine->first;
                        errors.push("ERRO NA LINHA " + tempSS.str() + ": SECTION TEXT ja foi declarada.");
                        tempSS.str("");
                    }
                    codeSection = TEXT;
                    achouSectionText = true;
                } else if ("DATA" == codeLine->second[1]) {
                    if (achouSectionData){
                        tempSS << dec << codeLine->first;
                        errors.push("ERRO NA LINHA " + tempSS.str() + ": SECTION DATA ja foi declarada.");
                        tempSS.str("");
                    }
                    codeSection = DATA;
                    achouSectionData = true;
                } else {
                    tempSS << dec << codeLine->first;
                    errors.push("ERRO NA LINHA " + tempSS.str() + ": O argumento "+ codeLine->second[1] + " eh invalido.");
                    tempSS.str("");
                }

            } else if ("SPACE" == codeLine->second.front()) {

                if (DATA != codeSection) {
                    tempSS << dec << codeLine->first;
                    errors.push("ERRO NA LINHA " + tempSS.str() + ": SPACE deve estar na SECTION DATA.");
                    tempSS.str("");
                }

                outputBSS.append("resd ");

                // Ve se eh um vetor
                if (codeLine->second.size() == 2) {
                    outputBSS.append(codeLine->second[1]);

                // Se nao reserva so um endereco
                } else {
                    outputBSS.append("1");
                }

                outputBSS.append("\n");


            } else if ("CONST" == codeLine->second.front()) {

                if (DATA != codeSection) {
                    tempSS << dec << codeLine->first;
                    errors.push("ERRO NA LINHA " + tempSS.str() + ": CONST deve estar na SECTION DATA.");
                    tempSS.str("");
                }

                outputData.append("dd "+codeLine->second[1]+"\n");

            } else if ("IF" == codeLine->second.front()) {

                if ("0" == codeLine->second[1]) {

                    cout << endl << "IF DEU FALSE" << endl;
                    ++codeLine;

                } else {

                    cout << endl << "IF DEU TRUE" << endl;

                }
            }

        
        } else if (":" == codeLine->second.front().substr(codeLine->second.front().length() - 1 , 1)) {

            string labelSemDoisPontos = codeLine->second.front().substr(0, codeLine->second.front().length() - 1);

            if ("SPACE" == codeLine->second[1])
                outputBSS.append(labelSemDoisPontos+" ");
            else if ("CONST" == codeLine->second[1])
                outputData.append(labelSemDoisPontos+" ");
            else
                outputText.append(codeLine->second.front()+" ");

            labels[labelSemDoisPontos] = memPos;
            codeLine->second.erase(codeLine->second.begin());
            --codeLine;

        // Diretiva/Instrucao nao existe
        } else {

            tempSS << codeLine->first;
            errors.push("ERRO NA LINHA " + tempSS.str() + ": A Diretiva/Instrucao "+codeLine->second.front() + " nao existe.");
            tempSS.str("");
        }
    }

    if (!achouSectionText) {
        errors.push("ERRO: A SECTION TEXT nao foi declarada.");
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
        cout << i->first << ": " << hex << i->second << endl;
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

    cout << endl;

    // Sem erros de traducao
    // Escreve o arquivo de saida
    if (errors.empty()) {

        char *file2 = (char*) malloc(sizeof(char)*strlen(argv[1]) + 1);
        strcpy(file2, argv[1]);
        strcat(file2, ".s");
        ofstream outputFile;
        outputFile.open(file2);
        outputFile << "section .text\nglobal _start\n_start:";
        outputFile << outputText << "\n";
        if (achouInput) 
            outputFile << "\nLerInteiro: \nmov eax, 3 \nmov ebx, 0 \nmov ecx, var \nmov edx, size \nint 0x80 \ncmp DWORD[var], 0xA \nje fim \nmov eax, [temp] \nmul DWORD[dez] \nmov ebx, [var] \nsub ebx, 0x30 \nadd eax, ebx \nmov [temp], eax \njmp LeerInteiro \nfim: \nmov eax, [temp] \nmov DWORD [temp], 0x0 \nret";

        if (achouOutput)
            outputFile << "\nEscreverInteiro: \npush ebx \npop eax \nmov cx,7 \nmov esi, var \nadd esi,7 ; tamanho da string \nmov cx,10 \nloop2: mov edx,0 \ndiv cx \nadd dx,30h \nmov [esi],dl \nsub esi,1 \ncmp ax,0 \nje fim \nconv2 \njmp loop2 \nfimconv2: mov eax,4 \nmov ebx,1 \nmov ecx, var \nmov edx, size \nint 80h \nret";        
        outputFile << "\nsection .data \n";
        outputFile << outputData << "\n";
        outputFile << "section .bss \n";
        outputFile << outputBSS << "\n";
        outputFile.close();

        char *file3 = (char*) malloc(sizeof(char)*strlen(argv[1]) + 1);
        strcpy(file3, argv[1]);
        strcat(file3, ".cod");
        ofstream outputFile2;
        outputFile2.open(file3);
        outputFile2 << codOutputText;
        outputFile2 << codOutputData;
        outputFile2 << codOutputBSS;
        outputFile2.close();

        /*abrir o .cod no modo r e no modo w

        while !feof(cod)

        fscanf()*/

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

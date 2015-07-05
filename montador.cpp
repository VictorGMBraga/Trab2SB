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
//#include <elfio/elfio.hpp>

using namespace std;
//using namespace ELFIO;

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
Labels          dataLabels;
Defines         defines;
string          outputText, outputData, outputBSS;
string          codOutputText, codOutputData, codOutputBSS;

const int textStartAddress = 0x08048080;
int dataStartAddress;

bool achouInput = false;
bool achouOutput = false;
/*
void criaElf ( unsigned char* text, unsigned char* data, char* fileName) {
    elfio writer;
    
    // You can't proceed without this function call!
    writer.create( ELFCLASS32, ELFDATA2LSB );

    writer.set_os_abi( ELFOSABI_LINUX );
    writer.set_type( ET_EXEC );
    writer.set_machine( EM_386 );

    // Create code section
    section* text_sec = writer.sections.add( ".text" );
    text_sec->set_type( SHT_PROGBITS );
    text_sec->set_flags( SHF_ALLOC | SHF_EXECINSTR );
    text_sec->set_addr_align( 0x10 );
    
    text_sec->set_data( text, sizeof( text ) );

    // Create a loadable segment
    segment* text_seg = writer.segments.add();
    text_seg->set_type( PT_LOAD );
    text_seg->set_virtual_address( 0x08048000 ); //Endereco que começa o section text
    text_seg->set_physical_address( 0x08048000 ); //Endereco || || || ||
    text_seg->set_flags( PF_X | PF_R );
    text_seg->set_align( 0x1000 );
    
    // Add code section into program segment
    text_seg->add_section_index( text_sec->get_index(), text_sec->get_addr_align() );

    // Create data section*
    section* data_sec = writer.sections.add( ".data" );
    data_sec->set_type( SHT_PROGBITS );
    data_sec->set_flags( SHF_ALLOC | SHF_WRITE );
    data_sec->set_addr_align( 0x4 );
    data_sec->set_data( data, sizeof( data ) ); //tamanho do vetor data

    // Create a read/write segment
    segment* data_seg = writer.segments.add();
    data_seg->set_type( PT_LOAD );
    data_seg->set_virtual_address( 0x08048020 ); //Endereco que comeca o section data
    data_seg->set_physical_address( 0x08048020 ); //Endereco que começa o section data
    data_seg->set_flags( PF_W | PF_R );
    data_seg->set_align( 0x10 );

    // Add code section into program segment
    data_seg->add_section_index( data_sec->get_index(), data_sec->get_addr_align() );

    section* note_sec = writer.sections.add( ".note" );
    note_sec->set_type( SHT_NOTE );
    note_sec->set_addr_align( 1 );
    note_section_accessor note_writer( writer, note_sec );

    // Setup entry point
    writer.set_entry( 0x08048000 ); //Endereço do global start

    // Create ELF file
    writer.save(fileName); //Nome do arquivo Executável

}
*/


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

    instructions["ADD"]         = make_tuple(1, "\nadd eax, [<ARG1>]",                            "03 05 <ADDR1>",                       6);
    instructions["SUB"]         = make_tuple(1, "\nsub eax, [<ARG1>]",                            "2B 05 <ADDR1>",                       6);
    instructions["MULT"]        = make_tuple(1, "\nimul eax, [<ARG1>]",                           "0F AF <ADDR1>",                       7);
    instructions["DIV"]         = make_tuple(1, "\nmov ebx, [<ARG1>]\nidiv ebx",                "8B 1D <ADDR1> F7 FB",                  8);
    instructions["JMP"]         = make_tuple(1, "\njmp <ARG1>",                                   "EB <RELADDR>",                        2);
    instructions["JMPN"]        = make_tuple(1, "\ncmp eax, 0\njl <ARG1>",                      "83 F8 00 7C <RELADDR>",                5);
    instructions["JMPP"]        = make_tuple(1, "\ncmp eax, 0\njg <ARG1>",                      "83 F8 00 7F <RELADDR>",                5);
    instructions["JMPZ"]        = make_tuple(1, "\ncmp eax, 0\nje <ARG1>",                      "83 F8 00 74 <RELADDR>",                5);
    instructions["COPY"]        = make_tuple(2, "\nmov eax, [<ARG1>]\nmov [<ARG2>], eax",       "A1 <ADDR1> A3 <ADDR2>",               10);
    instructions["LOAD"]        = make_tuple(1, "\nmov eax, [<ARG1>]",                            "A1 <ADDR1>",                          5);
    instructions["STORE"]       = make_tuple(1, "\nmov [<ARG1>], eax",                            "A3 <ADDR1>",                          5);
    instructions["INPUT"]       = make_tuple(1, "\nmov DWORD edi, [<ARG1>]\n call LerInteiro",   "E8 <ADDR1> ",                                     2); // ver
    instructions["OUTPUT"]      = make_tuple(1, "\nmov edi, [<ARG1>]\ncall EscreverInteiro",    " E8",                                     2); // ver
    instructions["STOP"]        = make_tuple(0, "\nmov eax, 1\nmov ebx, 0\nint 80h",          "B8 01 00 00 00 BB 00 00 00 00 CD 80",  12);

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
    CodeSection codeSection = NONE;

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

            if ("SECTION" == tempStr) {

                string tempStr2;
                iss >> tempStr2;

                if ("TEXT" == tempStr2)
                    codeSection = TEXT;
                else if ("DATA" == tempStr2)
                    codeSection = DATA;

                codeLines[lineCount].push_back(tempStr);
                codeLines[lineCount].push_back(tempStr2);

                continue;

            }

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
                            errors.push("ERRO NA LINHA " + tempSS.str() + ": EQU ja declarado.");
                            tempSS.str("");
                        } else {
                            // Coloca o valor do EQU na tabela de defines
                            defines[tempStr] = tempStr3;
                        }

                    // Se nao eh so um label
                    // Com algo a mais na mesma linha
                    } else {

                        if ( (labels.find(tempStr) != labels.end()) ||
                             (dataLabels.find(tempStr) != dataLabels.end())  ){
                            tempSS << lineCount;
                            errors.push("ERRO NA LINHA " + tempSS.str() + ": Label ja declarado.");
                            tempSS.str("");
                        } else {
                            // Adiciona na tabela de labels
                            if(codeSection == TEXT){
                                labels[tempStr] = textMemAddr;
                            } else if (codeSection == DATA) {

                                dataLabels[tempStr] = dataMemAddr;
                                dataMemAddr += 4;
                            }
                        }
                        
                        // Adiciona endereco de memoria
                        if (instructions.find(tempStr2) != instructions.end())
                            textMemAddr += get<3>(instructions[tempStr2]);

                        // Adiciona os tokens ao vetor
                        codeLines[lineCount].push_back(tempStr+":");
                        codeLines[lineCount].push_back(tempStr2);

                    }

                // Se nao eh um label "sozinho"
                // Adiciona no vetor
                } else {

                    // Remove o ':'
                    tempStr = tempStr.substr(0, tempStr.length() - 1);

                    if (labels.find(tempStr) != labels.end()){
                        tempSS << lineCount;
                        errors.push("ERRO NA LINHA " + tempSS.str() + ": Label ja declarado.");
                        tempSS.str("");
                    } else {
                        // Adiciona na tabela de labels
                        if(codeSection == TEXT){
                            labels[tempStr] = textMemAddr;
                        } else if (codeSection == DATA) {
                            dataLabels[tempStr] = dataMemAddr;
                            dataMemAddr += 4;
                        }
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

    dataStartAddress = textMemAddr + (4-(textMemAddr % 4));

}

// Compilacao em si. Gera o codigo objeto.
void compile () {

    bool achouSectionText = false;
    bool achouSectionData = false;
    CodeSection codeSection = NONE;
    stringstream tempSS;
    int memPos = 0x08048080;
    
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

            // Se a instrucao eh STOP nao existem operandos
            if ("STOP" == codeLine->second.front()) {

                outputText.append(get<1>(instructions[codeLine->second.front()]));
                codOutputText.append(get<2>(instructions[codeLine->second.front()]));

            } else {

                // Label do TEXT
                if (labels.find(codeLine->second[1]) != labels.end()){
                    
                    // Troca <ARG1> pelo nome do label
                    string auxStr = get<1>(instructions[codeLine->second.front()]);
                    strReplace(auxStr, "<ARG1>", codeLine->second[1]);

                    /*
                    // Troca <RELADDR> pela endereco relativo do label
                    string auxStr2 = get<2>(instructions[codeLine->second.front()]);
                    int relAddr = labels[codeLine->second[1]] - memPos;
                    tempSS << hex << relAddr;
                    strReplace(auxStr2, "<RELADDR>", tempSS.str());
                    tempSS.str("");
                    */

                    // Troca <ADDR1> pelo endereco do label
                    string auxStr2 = get<2>(instructions[codeLine->second.front()]);
                    tempSS << hex << dataLabels[codeLine->second[1]] + dataStartAddress;
                    strReplace(auxStr2, "<RELADDR>", tempSS.str().substr(tempSS.str().length() - 2, 2) + " " + tempSS.str().substr(tempSS.str().length() - 4, 2) + " " + tempSS.str().substr(tempSS.str().length() - 6, 2) + " 0" + tempSS.str().substr(tempSS.str().length() - 7, 1) );
                    tempSS.str("");

                    outputText.append(auxStr);
                    codOutputText.append(auxStr2);

                // LABEL do DATA
                } else if (dataLabels.find(codeLine->second[1]) != dataLabels.end()) {

                    // Troca <ARG1> pelo nome do label
                    string auxStr = get<1>(instructions[codeLine->second.front()]);
                    strReplace(auxStr, "<ARG1>", codeLine->second[1]);

                    // Troca <ADDR1> pelo endereco do label
                    string auxStr2 = get<2>(instructions[codeLine->second.front()]);
                    tempSS << hex << dataLabels[codeLine->second[1]] + dataStartAddress;
                    strReplace(auxStr2, "<ADDR1>", tempSS.str().substr(tempSS.str().length() - 2, 2) + " " + tempSS.str().substr(tempSS.str().length() - 4, 2) + " " + tempSS.str().substr(tempSS.str().length() - 6, 2) + " 0" + tempSS.str().substr(tempSS.str().length() - 7, 1) );
                    tempSS.str("");

                    // Se eh COPY, tem 2 operandos
                    if ("COPY" == codeLine->second.front()) {
                        
                        // Troca <ARG2> pelo nome do label
                        strReplace(auxStr, "<ARG2>", codeLine->second[2]);
                        
                        // Troca <ADDR2> pelo endereco do label
                        tempSS << hex << dataLabels[codeLine->second[2]] + dataStartAddress;
                        strReplace(auxStr2, "<ADDR2>", tempSS.str().substr(tempSS.str().length() - 2, 2) + " " + tempSS.str().substr(tempSS.str().length() - 4, 2) + " " + tempSS.str().substr(tempSS.str().length() - 6, 2) + " 0" + tempSS.str().substr(tempSS.str().length() - 7, 1));
                        tempSS.str("");

                        // Escreve nas saidas
                        outputText.append(auxStr);
                        codOutputText.append(auxStr2+" ");

                    // Nao eh copy
                    } else {

                        // Escreve nas saidas
                        outputText.append(auxStr);
                        codOutputText.append(auxStr2+" ");

                    }

                // LABEL inexistente
                } else {

                    tempSS << dec << codeLine->first;
                    errors.push("ERRO NA LINHA " + tempSS.str() + ": O Label "+ codeLine->second[1] + " nao existe.");
                    tempSS.str("");
                
                }

            }

            // Adiciona tamanho da instrucao ao endereco de memoria
            memPos += get<3>(instructions[codeLine->second.front()]);

        // Achou uma Diretiva
        } else if (directives.find(codeLine->second.front()) != directives.end()) {

            // DIRETIVA SECTION
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

            // DIRETIVA SPACE
            } else if ("SPACE" == codeLine->second.front()) {

                // Ve se SPACE nao esta na SECTION CODE
                if (DATA != codeSection) {
                    tempSS << dec << codeLine->first;
                    errors.push("ERRO NA LINHA " + tempSS.str() + ": SPACE deve estar na SECTION DATA.");
                    tempSS.str("");
                }

                outputBSS.append("resd ");

                // Ve se eh um vetor
                if (codeLine->second.size() == 2) {
                    outputBSS.append(codeLine->second[1]);

                    for (int i = 0; i < atoi(codeLine->second[1].c_str()); ++i) {
                        codOutputBSS.append(" 00");
                    }

                // Se nao reserva so um endereco
                } else {
                    outputBSS.append("1");
                    codOutputBSS.append(" 00");
                }

            // DIRETIVA CONST
            } else if ("CONST" == codeLine->second.front()) {

                if (DATA != codeSection) {
                    tempSS << dec << codeLine->first;
                    errors.push("ERRO NA LINHA " + tempSS.str() + ": CONST deve estar na SECTION DATA.");
                    tempSS.str("");
                }

                outputData.append("dd "+codeLine->second[1]+"\n");
                int tempint = atoi(codeLine->second[1].c_str());
                tempSS << hex << tempint;
                if(tempint > 15)
                    codOutputData.append(" "+tempSS.str());
                else
                    codOutputData.append(" 0"+tempSS.str());
                tempSS.str("");

            // DIRETIVA IF
            } else if ("IF" == codeLine->second.front()) {

                // Se IF eh false, pula a linha seguinte
                if ("0" == codeLine->second[1])
                    ++codeLine;

            }

            // Adiciona o tamanho da diretiva ao contador de posicao de memoria
            memPos += get<2>(directives[codeLine->second.front()]);

        // Achou um label
        // Reescreve ele na saida
        } else if (":" == codeLine->second.front().substr(codeLine->second.front().length() - 1 , 1)) {

            string labelSemDoisPontos = codeLine->second.front().substr(0, codeLine->second.front().length() - 1);

            if ("SPACE" == codeLine->second[1])
                outputBSS.append(labelSemDoisPontos+" ");
            else if ("CONST" == codeLine->second[1])
                outputData.append(labelSemDoisPontos+" ");
            else
                outputText.append("\n"+codeLine->second.front()+" ");

            codeLine->second.erase(codeLine->second.begin());
            
            if(!codeLine->second.empty())
                --codeLine;

        // Diretiva/Instrucao nao existe
        } else {

            tempSS << codeLine->first;
            errors.push("ERRO NA LINHA " + tempSS.str() + ": A Diretiva/Instrucao "+ codeLine->second[0] + " nao existe.");
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


    /*cout << endl << "# LABELS #" << endl;
    for (Labels::iterator i = labels.begin(); i != labels.end(); ++i) {
        cout << i->first << ": " << hex << i->second << endl;
    }

    cout << endl << "# LABELS DATA #" << endl;
    for (Labels::iterator i = dataLabels.begin(); i != dataLabels.end(); ++i) {
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
    }*/

    compile();

    /*cout << endl << "# SECTION TEXT #" << outputText << endl;
    cout << endl << "# SECTION DATA #" << endl << outputData << endl;
    cout << endl << "# SECTION BSS #" << endl << outputBSS << endl << endl;*/

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
            outputFile << "\nLerInteiro:\nenter 4,0\npush eax\nmov eax, 3\nmov ebx,0\nmov ecx, [ebp-4]\nmov edx, 16\nint 80h\nmov esi, [ebp-4]\nmov ecx, 0\nLoop1:\ncmp dword esi, 0Ah\nje fim\ncmp dword esi, 00h\nje fim\nsub esi, 30h\nimul ecx,ecx,10 \nadd ecx, ebx\ninc esi\njmp Loop1\nfim: mov [edi], ecx\npop eax\nleave\nret";
        if (achouOutput)
            outputFile << "\nEscreverInteiro: \npush ebx \npop eax \nmov cx,7 \nmov esi, var \nadd esi,7 \nmov cx,10 \nloop2: mov edx,0 \ndiv cx \nadd dx,30h \nmov [esi],dl \nsub esi,1 \ncmp ax,0 \nje fim \nconv2 \njmp loop2 \nfimconv2: mov eax,4 \nmov ebx,1 \nmov ecx, var \nmov edx, size \nint 80h \nret";        
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
        if (achouInput) 
            outputFile2 << "";
        outputFile2 << codOutputData;
        outputFile2 << codOutputBSS;
        outputFile2.close();


        /*cout << endl << "# COD EM NUMEROS #" << endl;*/
        string codOutputTemp = codOutputText;
        string dataOutputTemp =  codOutputData + codOutputBSS;
        
        stringstream tempSS(codOutputTemp);
        stringstream tempSS2(dataOutputTemp);

        //cout << "TAMANHO DA SS: " << tempSS.rdbuf()->in_avail() << endl;

        unsigned char *buffer1 = NULL;
        unsigned char *buffer2 = NULL;

        buffer1 = (unsigned char*)calloc(tempSS.rdbuf()->in_avail(), sizeof(unsigned char));
        buffer2 = (unsigned char*)calloc(tempSS2.rdbuf()->in_avail(), sizeof(unsigned char));

        int cont = 0;
        string tempStr;
        while (tempSS >> tempStr) {
            unsigned char tempChar = (unsigned char)strtol(tempStr.c_str(), NULL, 16);
            //cout << hex << (unsigned int)tempChar << " ";
            buffer1[cont] = tempChar;
        }

        cont = 0;
        while (tempSS2 >> tempStr) {
            unsigned char tempChar = (unsigned char)strtol(tempStr.c_str(), NULL, 16);
            //cout << hex << (unsigned int)tempChar << " ";
            buffer2[cont] = tempChar;
        }


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

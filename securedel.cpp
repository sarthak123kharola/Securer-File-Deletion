#include<iostream>
#include<fstream>
#include<random>
#include<string>
#include<vector>
#include<cstdio>
using namespace std;

void SecureDelete(const string&filepath){
    ifstream file(filepath, ios::binary | ios::ate);
    if(!file){
        cerr<<"Error: File not found. Please try again!\n";
        return;
    }
    streamsize filesize= file.tellg();
    file.close();
    if(filesize==0){
        cerr<<"Error: File is empty. Please try again!\n";
        return;
    }
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<unsigned char> dist(0, 255);
    ofstream fileout(filepath, ios::binary);
    if(!fileout){
        cout<<"Error: Unable to open file file for writting!\n";
        return;
    }
    int passes= 7;
    vector<unsigned char> buffer(filesize);
    for(int i=0; i<passes; i++){
        for(auto &iter: buffer){
            iter= dist(gen);
            fileout.seekp(0);
            fileout.write(reinterpret_cast<const char *>(buffer.data()), buffer.size());
            fileout.flush();
        }
    }
    fileout.close();
    if(remove(filepath.c_str())==0) cout<<"File securely deleted!\n";
    else cout<<"Error: Failed to delete file. Please try again!\n";
}

int main(){
    while(1){
    string filepath;
    cout<<"Enter file path:";
    getline(cin, filepath);
    SecureDelete(filepath);
    }
    return 0;
}
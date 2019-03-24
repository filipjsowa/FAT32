#include <iostream>
#include <fstream>
#include <iomanip>
#include <cstring>
#include <vector>


class Sector;
class Cluster;
class MBR;
class FAT;
class File;

std::stringstream getFileName(unsigned char * sector, long long offset, long long length);
long long hexToInt(unsigned char * sector, long long offset, long long size_in_bytes);
long long LittleEndianHexToInt(unsigned char * sector, long long offset, long long size_in_bytes);
void IntToLittleEndianHex(unsigned char * buff, long long num);
std::string getHex(unsigned char * arr, long long length, long long cuts);
std::string getASCII(unsigned char * arr, long long length, long long cuts);


long long hexToInt(unsigned char * sector, long long offset, long long size_in_bytes){
    long long result;
    std::stringstream ss;
    ss <<std::hex << std::setfill('0');
    for(long long i=0; i<size_in_bytes; i++){
        ss << std::setw(2) << (long long)sector[offset+i];
    }
    ss >> result;
    return result;
}

long long LittleEndianHexToInt(unsigned char * sector, long long offset, long long size_in_bytes){
    long long result;
    if(size_in_bytes == 4){
        result = ( (sector[offset]<<0) | (sector [1+ offset] << 8) | (sector[2 + offset] <<16 ) | (sector[3+offset]) <<24 );
    }
    if(size_in_bytes == 2){
        result = ( (sector[offset]<<0 ) | (sector[1+offset]<<8));
    }
    if(size_in_bytes==1){
        result = sector[offset];
    }

    return result;
}

void IntToLittleEndianHex(unsigned char * buff, long long num){
    num =  __builtin_bswap32(num);
    buff[0] = (num >> 24) & 0xFF;
    buff[1] = (num >> 16) & 0xFF;
    buff[2] = (num >> 8) & 0xFF;
    buff[3] = num & 0xFF;

}


class Sector{
public:
    long long m_sector_number;
    unsigned char* m_sector;
    long long m_sector_size = 512;
    std::fstream *m_file;
    std:: vector <File> files;



    Sector(){
        m_sector_number = 0;
        m_sector= nullptr;
        m_file = nullptr;
    }

    Sector(std::fstream *file, long long sector_number){
        m_file = file;
        m_sector_number = sector_number;
        m_sector = new unsigned char[m_sector_size];
        file->seekg(m_sector_number*m_sector_size);
        file->read((char*)m_sector, m_sector_size );
        for(int i=0; i< m_sector_size/32; i++){
            files.emplace_back(getFileName(m_sector, i*32, 32).str(), m_file, m_sector_number*m_sector_size +i*32 );
        }
    }

    Sector(const Sector & temp){
        m_sector = new unsigned char [m_sector_size];
        memcpy(m_sector, temp.m_sector, m_sector_size);
        m_file = temp.m_file;
        m_sector_number = temp.m_sector_number;
        files = temp.files;
    }

    Sector& operator=(Sector const& temp){
        delete[] m_sector;
        m_sector = new unsigned char [m_sector_size];
        memcpy(m_sector, temp.m_sector, m_sector_size);
        m_file = temp.m_file;
        m_sector_number = temp.m_sector_number;
        files = temp.files;
        return *this;
    }

    ~Sector(){
        delete[] m_sector;
        m_sector = nullptr;
    }

    void FlushAndReload(){
        m_file->seekp(m_sector_number*m_sector_size);
        m_file->write((char*)m_sector, m_sector_size);
    }

    void UptadeSector();
};

class MBR : public Sector{
public:
    unsigned char ** m_partitions;
    long long * m_FAT32VolumeIDs;


    explicit MBR(std::fstream *file) :Sector(file, 0) {
        m_partitions = new unsigned char *[4];
        for (long long i = 0; i < 4; i++) {
            m_partitions[i] = new unsigned char[16];
        }

        for(long long i=0; i<64; i++){
            m_partitions[i/16][i%16] = m_sector[i+446];
        }

        m_FAT32VolumeIDs = new long long [4];

        for(long long i=0; i<4; i++){
            m_FAT32VolumeIDs[i] = LittleEndianHexToInt(m_partitions[i], 8, 4);
        }
    }


    ~MBR(){
        for (long long i = 0; i < 4; i++) {
            delete [] m_partitions[i];
        }
        delete[] m_partitions;
        delete[] m_FAT32VolumeIDs;
    }
};

class FAT{
public:
    std::vector <long long> m_FileAllocationTable;
    long long m_FatSize;
    std::fstream * m_file;
    long long m_FatStart;
    FAT(long long FatStart, long long Sectors_per_fat, std::fstream * file){
        for(long long i=0; i<Sectors_per_fat; i++){
            Sector temp(file, FatStart+i);
            for(long long j=0; j< 128; j++){
                m_FileAllocationTable.push_back( LittleEndianHexToInt(temp.m_sector, j*4, 4));
//                m_FileAllocationTable.push_back( hexToInt(temp.m_sector, j*4, 4));
            }
        }
        m_FatSize = Sectors_per_fat * 128;
        m_file = file;
        m_FatStart = FatStart;
    }

    long long getNextCluster(long long cluster){
        if(m_FileAllocationTable[cluster+2] >= 0x0FFFFFF8) {
            return m_FileAllocationTable[cluster + 2];
        } else{
            return m_FileAllocationTable[cluster + 2];
        }
    }
    FAT(){
        m_FatSize = 0;
    }

    FAT operator=(FAT const& temp){
        m_FileAllocationTable = temp.m_FileAllocationTable;
        m_FatSize = temp.m_FatSize;
        return *this;
    }

    long long getFreeCluster(){
        long long i = 0;
        while (true){
            if(i>m_FatSize){
                std::cerr << "brak miejsca na dysku" << std::endl;
                return -1;
            }
            if(m_FileAllocationTable[i]==0){
                return i;
            } else{
                i++;
            }
        }
    }


    long long getNextFreeCluster(long long curr){
        long long i = curr+1;
        while (true){
            if(i>m_FatSize){
                std::cerr << "brak miejsca na dysku" << std::endl;
                return -1;
            }
            if(m_FileAllocationTable[i]==0){
                return i;
            } else{
                i++;
            }
        }
    }

    void setNextCluster(long long pos, long long next_cluster){
        m_FileAllocationTable[pos] = next_cluster;
        m_file->seekp(m_FatStart * 512 + pos * 4 , std::ios_base::beg);

        unsigned char buff[4];
        IntToLittleEndianHex(buff, next_cluster);
        m_file->write((char*)buff, 4);
        m_file->seekp(m_FatStart * 512 + pos * 4 +m_FatSize * 4 , std::ios_base::beg);
        m_file->write((char*)buff, 4);


    }

};

class Cluster{
public:
    std::vector<Sector> m_sectors;
    std::vector<File> files;
    long long m_Cluster_Number;
    long long m_Sectors_Per_Cluster;
    long long m_Cluster_Begin;
    std::fstream *m_file;
    Cluster(std::fstream * file,long long Cluster_Number, long long Sectors_Per_Cluster, long long Cluster_Begin){
        m_file =file;
        m_Cluster_Number = Cluster_Number;
        m_Sectors_Per_Cluster = Sectors_Per_Cluster;
        m_Cluster_Begin = Cluster_Begin;
        if(m_Cluster_Number<0)
            m_Cluster_Number=0;
        for(long long i=0; i<Sectors_Per_Cluster; i++){
            m_sectors.emplace_back(file, Cluster_Begin + m_Cluster_Number*Sectors_Per_Cluster + i);
        }
        for(auto temp : m_sectors){
            files.insert(files.end(), temp.files.begin(), temp.files.end());
        }
    }

    Cluster(){
        m_Cluster_Begin = 0;
        m_Sectors_Per_Cluster = 0;
        m_Cluster_Number = 0;
        m_file = nullptr;
    }

    void Flush(){
        for(int i=0; i< m_Sectors_Per_Cluster; i++){
            m_sectors[i].m_sector_number = m_Cluster_Begin + m_Cluster_Number*m_Sectors_Per_Cluster + i;
            m_sectors[i].FlushAndReload();
        }
    }


    Cluster(const Cluster & temp)=default;
    Cluster& operator=(Cluster const& temp)= default;

};

class File{
public:
    std::string fileRecord;
    std::string LongFileName;
    unsigned char LFN_Mask = 15;
    unsigned char Dir_mask = 16;
    unsigned char Hidd_mask = 2;
    unsigned char System_mask = 4;
    std::fstream * m_file;
    long long m_position;

    bool isLongFileName(){
        return((((unsigned char)fileRecord[11]) & LFN_Mask) == LFN_Mask);
    }

    bool isUnused(){
        return (((unsigned char)fileRecord[0]) == 0xE5);
    }

    bool  isDirectory(){
        return((((unsigned char)fileRecord[11]) & Dir_mask) == Dir_mask);
    }

    std::string getFileName(){
        if(LongFileName.size()){
            return LongFileName;
        }
        std::string temp = fileRecord.substr(0, 11);
        if(temp.size()>8 && !isDirectory()){
            temp.insert(8, 1,  '.');

            unsigned i = temp.find(' ');
            unsigned j = temp.rfind(' ');
            temp.erase(i, j-i+1);
        }
        if(isDirectory()){
            unsigned i = temp.find(' ');
            unsigned j = temp.rfind(' ');
            temp.erase(i, j-i+1);
        }
        return temp;
    }

    std::string getLongFileName(){
        bool lfn[32] = {0,1,0,1,0,1,0,1,0,1,0,0,0,0,1,0,1,0,1,0,1,0,1,0,1,0,0,0,1,0,1,0};
        std::stringstream ss;
        for(long long i=0; i<32; i++){
            if(lfn[i]){//long File names are null terminated. but you've got to guess it
                //and they're not even in ASCII for god's sake
                if((long long)(unsigned char)fileRecord[i]) {
                    ss << fileRecord[i];
                } else{
                    break;
                }
            }
        }
        return ss.str();
    }

    bool isEmpty(){
        return ((unsigned char)fileRecord[0]) == 0;
    }

    bool  isHidden(){
        return((((unsigned char)fileRecord[12]) & Hidd_mask) == Hidd_mask);
    }

    bool isSystem(){
        return((((unsigned char)fileRecord[12]) & System_mask) == System_mask);
    }

    long long clusterNum(){
        unsigned char num[4];
        num[0] = (unsigned char)fileRecord[0x1A];
        num[1] = (unsigned char)fileRecord[0x1B];
        num[2] = (unsigned char)fileRecord[0x14];
        num[3] = (unsigned char)fileRecord[0x15];
        return LittleEndianHexToInt(num, 0, 4);
    }


    long long Size(){
        unsigned char size[4];
        for(int i=0; i<4; i++){
            size[i] = (unsigned char)fileRecord[0x1C +i];
        }
        return LittleEndianHexToInt(size, 0, 4);
    }
    void updateName(std::string name){
        for(int i=0; i<11 || i<name.size(); i++){
            fileRecord[i] = name[i];
        }
        Flush();
    }

    void Delete(){
        fileRecord[0] = 0xe5;
        Flush();
    }

    unsigned char getAttr(){
        return (unsigned char)fileRecord[0x0b];
    }


    void Flush(){
        m_file->seekp(m_position);
        m_file->write(fileRecord.c_str(), 32);
    }
    File(std::string temp, std::fstream * file, long long position){
        fileRecord = temp;
        m_file  =file;
        m_position = position;
    }

    File() = default;
    File(const File & temp)= default;
    File& operator=(File const& temp)= default;
    bool operator==(File const& temp){
        return fileRecord == temp.fileRecord;
    };

    ~File()= default;
};


void Sector::UptadeSector() {
    for (int i=0; i<m_sector_size; i++){
        m_sector[i] = (unsigned char)files[i/32].getFileName()[i];
    }
}

std::string getHex(unsigned char * arr, long long length, long long cuts=512){
    std::stringstream ss;
    ss << std::hex << std::setfill('0') ;
    for(long long i=0; i< length; i++){
        ss << std::setw(2) <<(long long)arr[i] <<" ";
        if((i+1)%cuts == 0 && i>10){
            ss<<std::endl;
        }
    }
    ss<< std::endl;
    return ss.str();
}

std::string getASCII(unsigned char * arr, long long length, long long cuts=512){
    std::stringstream ss;
    for(long long i=0; i<length; i++){
        ss<<(char)arr[i];
        if((i+1)%cuts == 0 && i>10){
            ss<<std::endl;
        }
    }
    ss<< std::endl;
    return ss.str();
}

std::stringstream getFileName(unsigned char * sector, long long offset, long long length){
    std::stringstream ss;
    for(long long i=0; i< length; i++){
        ss << (char)sector[i+offset];
    }
    return ss;
}

std::vector<Cluster> getClusterChain(Cluster dir, FAT * fat){
    std::vector<Cluster> temp;
    temp.push_back(dir);

    while(true){
        if(fat->getNextCluster(temp.back().m_Cluster_Number)<0x0FFFFFF8 ){
            temp.emplace_back(temp.back().m_file,fat->getNextCluster(temp.back().m_Cluster_Number)-2 , temp.back().m_Sectors_Per_Cluster, temp.back().m_Cluster_Begin);
        } else{
            return temp;
        }
    }
}

std::vector<File> getFileLists(Cluster dir, FAT * fat){
    std::vector<File> FileList;
    std::vector<Cluster> chain = getClusterChain(dir, fat);
    for(auto direc : chain) {
        for (auto i = direc.files.begin(); i < direc.files.end(); i++) {
            if (i->isEmpty()) {
                return FileList;
            }
            if (i == direc.files.begin()) {
                if (!i->isUnused() and !i->isLongFileName() and !i->isHidden() and !i->isSystem()) {
                    FileList.push_back(*i);
                }
            } else {
                if (!i->isUnused() and !i->isLongFileName() and !i->isHidden() and !i->isSystem()) {
                    auto temp = i;
                    temp--;
                    if (temp->isLongFileName()) {
                        std::stringstream ss;
                        for (; temp >= direc.files.begin() && temp->isLongFileName(); temp--) {
                            ss << temp->getLongFileName();
                        }
                        i->LongFileName = ss.str();
                        FileList.push_back(*i);
                    } else {
                        FileList.push_back(*i);
                    }
                }
            }
        }
    }
}

void ls(Cluster dir, FAT *fat){
    std::vector<File> files = getFileLists(dir, fat);
    for(auto xd : files){
        std::cout << xd.getFileName() << std::endl;
    }
}

long long getClusterNumber(Cluster dir, FAT* fat, std::string name){
    std::vector<File> files = getFileLists(dir, fat);
    for(auto xd : files){
        if(name == xd.getFileName()){
            if(xd.isDirectory()) {
                return xd.clusterNum();
            } else{
                std::cout << name << " nie jest folderem\n";
                return  dir.m_Cluster_Number +2;
            }
        }
    }
    std::cout << "brak takiego folderu\n";
    return  dir.m_Cluster_Number +2;
}

void touch(Cluster dir, FAT* fat, std::string fileName){
    std::vector<Cluster> chain = getClusterChain(dir, fat);
    for(auto temp : chain){
        for(auto file : temp.files){
            if(file.isEmpty()){
                file.updateName(fileName);
                return;
            }
        }
    }
}

std::string makeFileRecord(std::string dirName, long long clusterNumber, unsigned char attr, long long size){
    std::stringstream fileRecordDir;
    unsigned char clusterInBytes[4];
    IntToLittleEndianHex(clusterInBytes, clusterNumber);

    if(dirName.size()>11){
        dirName = dirName.substr(0, 10);
    }
    fileRecordDir << dirName;
    if(dirName.size()<11){
        for(int i=dirName.size(); i<11; i++){
            fileRecordDir << ' ';
        }
    }

    //tutaj atrybut

    fileRecordDir << (char) attr;


    //tutaj by byl czas
    for(long long i=0; i<8; i++){ //obsluge czasu dodam pozniej. jak bede mial czas
        fileRecordDir << (char)0;
    }


    //tutaj cluster high
    fileRecordDir << clusterInBytes[2];
    fileRecordDir << clusterInBytes[3];


    //tutaj chyba tez czas by byl?
    for(long long i=0; i<4; i++){
        fileRecordDir << (char)0;
    }

    //tutaj cluster low
    fileRecordDir << clusterInBytes[0];
    fileRecordDir << clusterInBytes[1];

    //tutaj rozmiar
    unsigned char sizeInBytes[4];
    IntToLittleEndianHex(sizeInBytes, size);

    for(long long i=0; i<4; i++){
        fileRecordDir << sizeInBytes[i];
    }
    return fileRecordDir.str();
}

void mkdir(Cluster dir, FAT* fat, std::string dirName){
    long long freeCluster = fat->getFreeCluster();
    fat->setNextCluster(freeCluster, 0x0FFFFFFF);
    std::string dirRecord = makeFileRecord(dirName, freeCluster, (char)0x10, 0);
    std::string dirItself = makeFileRecord(".", freeCluster, (char)0x10, 0);
    std::string parent = makeFileRecord("..", dir.m_Cluster_Number +2, (char)0x10, 0);


    touch(dir, fat, dirRecord);
    Cluster newDir(dir.m_file,freeCluster -2, dir.m_Sectors_Per_Cluster,dir.m_Cluster_Begin);
    touch(newDir, fat, dirItself);
    Cluster newDir1(dir.m_file,freeCluster  -2, dir.m_Sectors_Per_Cluster,dir.m_Cluster_Begin);
    touch(newDir1, fat,parent);
}

void cp(Cluster dir, FAT * fat, std::string File1, std::string File2){
    std::vector<File> files = getFileLists(dir, fat);
    for(auto i : files){
        if(i.getFileName() == File1){
            long long freeCluster = fat->getFreeCluster();
            fat->setNextCluster(freeCluster, 0x0FFFFFFF);
            std::string fileRecord = makeFileRecord(File2,freeCluster, i.getAttr(), i.Size());

            touch(dir, fat, fileRecord);
            Cluster first(dir.m_file, i.clusterNum() - 2, dir.m_Sectors_Per_Cluster, dir.m_Cluster_Begin);
            Cluster temp;
            for (int j = 0; j < i.Size(); ++j) {
                temp = first;
                temp.m_Cluster_Number = freeCluster -2;
                temp.Flush();
                if(fat->getNextCluster(first.m_Cluster_Number)<0x0FFFFFF8 ){
                    first = Cluster(first.m_file,fat->getNextCluster(first.m_Cluster_Number)-2 , first.m_Sectors_Per_Cluster, first.m_Cluster_Begin);
                    fat->setNextCluster(freeCluster,freeCluster+1);
                    freeCluster = fat->getFreeCluster();

                } else{
                    fat->setNextCluster(freeCluster, 0x0FFFFFFF);
                    return;
                }
            }
            return;
        }
    }
    std::cout << "plik " << File1 << " nie istnieje\n";
}

void rm(Cluster dir, FAT * fat, std::string fileName, bool silentRm = false){
    std::vector<File> files = getFileLists(dir, fat);
    for(auto xd : files){
        if(xd.getFileName() == fileName){
            if(xd.isDirectory()){
                if(!silentRm)
                std::cout << "plik jest folderem, uzyj rmdir\n";
            } else {
                xd.Delete();
            }
            return;
        }
    }
    if (!silentRm)
    std::cout << "brak takiego pliku\n";
}

void rmdir(Cluster dir, FAT * fat, std::string fileName, bool silentRm = false){
    std::vector<File> files = getFileLists(dir, fat);
    for(auto xd : files){
        if(xd.getFileName() == fileName){
            if(xd.isDirectory()){
                Cluster temp(dir.m_file, xd.clusterNum() - 2, dir.m_Sectors_Per_Cluster, dir.m_Cluster_Begin);
                std::vector<File> tempfiles = getFileLists(temp, fat);
                if(tempfiles.size()>2 && !silentRm){
                    std::cout << "folder nie jest pusty. Uzyj rmdir-r\n";
                    return;
                }
                for(auto dx : tempfiles){
                    if(dx.clusterNum() -2 != temp.m_Cluster_Number && dx.clusterNum() -2 != dir.m_Cluster_Number && dx.clusterNum()!=0) {
                        if (dx.isDirectory()) {
                            rmdir(temp, fat, dx.getFileName(), 1);
                        } else {
                            rm(temp, fat, dx.getFileName());
                        }
                    } else{
                        //file entry is pointing to parent dir or own dir. we dont want to get a recursive loop
                        dx.Delete();
                    }
                }
                xd.Delete();
            } else {
                std::cout << "plik jest nie folderem, uzyj rm\n";
            }
            return;
        }
    }
    std::cout << "brak takiego folderu\n";
}

std::string get8_3Filename(std::string temp){
    for(auto & c : temp){
        c = toupper(c);
    }

    if(temp.size()>12){
        temp = temp.substr(0, 10);
    }
    size_t i = temp.find('.');
    if(i==std::string::npos){
        return temp;
    }
    std::string extension = temp.substr(i+1, temp.size());
    temp = temp.substr(0, i);
    if(temp.size()<8){
        std::stringstream temp1;
        temp1 << temp;
        while (temp1.str().size()<8){
            temp1 << ' ';
        }
        temp = temp1.str();
    }
    temp += extension;
    return temp;
}


int main() {
    std::string PenName = "pen.dd";
//    std::cout << "podaj nazwe pliku\n";
//    std::getline(std::cin, PenName);
    std::fstream  file;
    file.open(PenName, std::ios_base::binary | std::ios_base::out | std::ios_base::in );
    MBR mbr(&file);;
    unsigned long Partition_LBA_Begin = mbr.m_FAT32VolumeIDs[0];
    Sector firstVol(&file, Partition_LBA_Begin);
    unsigned long Bytes_per_Sector = LittleEndianHexToInt(firstVol.m_sector, 0x0B, 2);
    unsigned long Sectors_Per_Cluster = LittleEndianHexToInt(firstVol.m_sector, 0x0D, 1);
    unsigned long Number_of_fats = LittleEndianHexToInt(firstVol.m_sector, 0x10, 1);
    unsigned long Number_of_Reserved_Sectors = LittleEndianHexToInt(firstVol.m_sector, 0x0E, 2);
    unsigned long Sectors_Per_FAT = LittleEndianHexToInt(firstVol.m_sector, 0x24, 4);
    unsigned long cluster_lba_begin = Partition_LBA_Begin + Number_of_Reserved_Sectors + (Number_of_fats * Sectors_Per_FAT);
    unsigned long root_dir_first_cluster = LittleEndianHexToInt(firstVol.m_sector,	0x2C, 4 );

    FAT fat(Partition_LBA_Begin+Number_of_Reserved_Sectors,Sectors_Per_FAT ,&file);

    std::string command;
    std::string fileName;
    long long currentClusterNumber = root_dir_first_cluster -2;

    Cluster current_dir;
    while(true) {

        current_dir = Cluster(&file, currentClusterNumber, Sectors_Per_Cluster, cluster_lba_begin);
        std::getline(std::cin, command);
        unsigned i = command.find(' ');
        fileName = command.substr(0, i);
        if(fileName == "ls"){
            ls(current_dir, &fat);
        } else if(fileName=="cd"){
            fileName = command.substr(i+1, command.size());
            currentClusterNumber = getClusterNumber(current_dir, &fat, fileName) -2;

        } else if(fileName=="cp"){
            unsigned  helper = command.rfind(' ');
            fileName = command.substr(i+1, helper-3);
            std::string temp1 = command.substr(helper+1, command.size());
            temp1 = get8_3Filename(temp1);
            std::string temp1withoutspaces = temp1;
            size_t j = temp1withoutspaces.find(' ');
            size_t k = temp1withoutspaces.rfind(' ');
            if(j!= std::string::npos){
                temp1withoutspaces.erase(j, k -j +1);
                temp1withoutspaces.insert(temp1withoutspaces.end() -3 ,1,  '.');
            }
            rm(current_dir, &fat,temp1withoutspaces, 1 );

            current_dir = Cluster(&file, currentClusterNumber, Sectors_Per_Cluster, cluster_lba_begin);

            cp(current_dir, &fat, fileName, temp1);

        } else if(fileName =="rm"){
            fileName = command.substr(i+1, command.size());
            rm(current_dir, &fat, fileName);

        } else if (fileName == "mkdir"){
            fileName = command.substr(i+1, command.size());
            if(fileName.size()>11){
                fileName = fileName.substr(0, 10);
            }
            fileName = get8_3Filename(fileName);
            mkdir(current_dir, &fat, fileName);

        } else if(fileName == "rmdir"){
            fileName = command.substr(i+1, command.size());
            rmdir(current_dir, &fat, fileName);

        } else if(fileName == "rmdir-r"){
            fileName = command.substr(i+1, command.size());
            rmdir(current_dir, &fat, fileName, 1);

        } else if(fileName == "touch"){
            fileName = command.substr(i+1, command.size());
            fileName = get8_3Filename(fileName);
            fileName =  makeFileRecord(fileName, fat.getFreeCluster(),0x20, 0);
            fat.setNextCluster(fat.getFreeCluster(), 0x0FFFFFFF );
            touch(current_dir, &fat, fileName);
        }
        else if(fileName == "exit"){
            file.close();
            return 0;
        }
        else{
            std::cout << "niepoprawna komenda\n";
        }
    }
}
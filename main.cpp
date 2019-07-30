#include <iostream>
#include "dcmtk/dcmdata/dctk.h"
#include "opencv2/core/core.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/imgproc.hpp"
#include "dcmtk/dcmrt/drtstrct.h"
#include "dcmtk/dcmdata/dcjson.h"
#include <dirent.h>
#include <sys/stat.h>
#include <map>

DcmFileFormat rtFileFormat;
std::map<OFString, Sint16> roiName2ROINumber;

int getXYZOrigin(DcmFileFormat fileformat, Float64 &xOrigin,Float64 &yOrigin, Float64 &zOrigin){
    DcmDataset *dataset = fileformat.getDataset();
    // OFString val;
    // float xyzOrigin[3];
    if (dataset->findAndGetFloat64(DCM_ImagePositionPatient, xOrigin, 0).good());
        
    if (dataset->findAndGetFloat64(DCM_ImagePositionPatient, yOrigin, 1).good());
        
    if (dataset->findAndGetFloat64(DCM_ImagePositionPatient, zOrigin, 2).good());
        
    return 0;
}

int getXYSpacing(DcmFileFormat fileformat, Float64 &xSpacing, Float64 &ySpacing){
    // float xSpacing, ySpacing;
    DcmDataset *dataset = fileformat.getDataset();
    if (dataset->findAndGetFloat64 (DCM_PixelSpacing, xSpacing, 0).good());
    if (dataset->findAndGetFloat64 (DCM_PixelSpacing, ySpacing, 1).good());
    return 0;
}

int getSOPInstanceUID(DcmFileFormat fileformat, OFString &UID){
    if ( fileformat.getDataset()->findAndGetOFString(DCM_SOPInstanceUID, UID).bad()) return 0;
    return 1;
}

int add2RTFile(OFString ROIName, Sint16 ROINumber,bool checkIsExist ,OFString UID, std::vector<Float64> cData){
    DcmDataset *dataset = rtFileFormat.getDataset();
    OFCondition cond;

    DcmItem *StructureSetROISequence = new DcmItem();
    DcmItem *ROINumberItem = new DcmItem();

    // std::cout << "ROINumber : " << ROINumber << "\n"; // Debug
    std::string val = std::to_string(ROINumber);
    cond = StructureSetROISequence->putAndInsertString (DCM_ROINumber, val.c_str());
    
    if (cond.bad()) return std::cout << cond.text(), -1;
    
    cond = StructureSetROISequence->putAndInsertOFStringArray (DCM_ROIName, ROIName, false);
    if (cond.bad()) return std::cout << "StructureSetROISequence->putAndInsertOFStringArray (DCM_ROIName, ROIName, ROINumber)", -1;
    cond = dataset->insertSequenceItem(DCM_StructureSetROISequence, StructureSetROISequence);
    if (cond.bad()) return std::cout << "dataset->insertSequenceItem(DCM_StructureSetROISequence, StructureSetROISequence)" , -1;

    DcmItem *ReferencedFrameOfReferenceSequence = new DcmItem();
    DcmItem *RTReferencedStudySequence = new DcmItem();
    cond = RTReferencedStudySequence->putAndInsertOFStringArray(DCM_ReferencedSOPInstanceUID, UID);
    if (cond.bad()) return std::cout << "RTReferencedStudySequence->putAndInsertOFStringArray(DCM_ReferencedSOPInstanceUID, UID)", -1;
    cond = ReferencedFrameOfReferenceSequence -> insertSequenceItem(DCM_RTReferencedStudySequence, RTReferencedStudySequence);
    if (cond.bad()) return std::cout << "ReferencedFrameOfReferenceSequence -> insertSequenceItem(DCM_RTReferencedStudySequence, RTReferencedStudySequence)", -1;
    cond = dataset->insertSequenceItem(DCM_ReferencedFrameOfReferenceSequence, ReferencedFrameOfReferenceSequence);
    if (cond.bad()) return std::cout << "dataset->insertSequenceItem(DCM_ReferencedFrameOfReferenceSequence, ReferencedFrameOfReferenceSequence)", -1;

    DcmItem *ROIContourSequence = new DcmItem();
    DcmItem *ContourSequence = new DcmItem();
    DcmItem *ContourImageSequence = new DcmItem();
    cond = ContourImageSequence->putAndInsertOFStringArray(DCM_ReferencedSOPInstanceUID, UID);
    if (cond.bad()) return std::cout << "ContourImageSequence->putAndInsertOFStringArray(DCM_ReferencedSOPInstanceUID, UID)", -1;
    
    //push contour data to RT structure set
    OFString valc;
    for (int i = 0; i < cData.size(); i++){
        valc = valc + std::to_string(cData.at(i)).c_str() + "\\";
    }
    ContourSequence->putAndInsertString(DCM_NumberOfContourPoints, std::to_string(cData.size()).c_str());
    ContourSequence->putAndInsertOFStringArray(DCM_ContourData, valc);
    
    // if (ContourSequence->tagExistsWithValue(DCM_ContourData)) std::cout << "YES\n"; // Debug
    if (cond.bad()) return std::cout << cond.text(), -1;

    cond = ContourSequence->insertSequenceItem(DCM_ContourImageSequence, ContourImageSequence);
    if (cond.bad()) return std::cout << "ContourSequence->insertSequenceItem(DCM_ContourImageSequence, ContourImageSequence)", -1;
    cond = ROIContourSequence->insertSequenceItem(DCM_ContourSequence, ContourSequence);
    if (cond.bad()) return std::cout << "ROIContourSequence->insertSequenceItem(DCM_ContourSequence, ContourSequence)", -1;
    cond = dataset->insertSequenceItem(DCM_ROIContourSequence, ROIContourSequence);
    if (cond.bad()) return std::cout << "dataset->insertSequenceItem(DCM_ROIContourSequence, ROIContourSequence)", -1;

    return 1;
}

int writeToDataset(OFString ROIName, OFString fileName, std::vector<std::vector<cv::Point>> contourData){
    OFString path = "../imageDCM/" + fileName;

    DcmFileFormat fileformat;
    if (fileformat.loadFile(path).bad()){
        std::cout << "can't load image : " << path << "\n";
    }

    Float64 xOrigin, yOrigin, zOrigin, xSpacing, ySpacing;
    OFString UID;
    // std::cout << "find xyz done!\n";
    getXYZOrigin(fileformat, xOrigin, yOrigin, zOrigin);
    getXYSpacing(fileformat, xSpacing, ySpacing);
    getSOPInstanceUID(fileformat, UID);
    
    std::vector<Float64> cData;
    for (int i = 0; i < contourData.size(); i++){
        std::vector<cv::Point> points = contourData.at(i);
        for (int j = 0; j < points.size(); j++){
            cv::Point p = points.at(j);
            Float64 x = p.x * xSpacing + xOrigin;
            Float64 y = p.y * ySpacing + yOrigin;
            Float64 z = zOrigin;
            cData.push_back(x);
            cData.push_back(y);
            cData.push_back(z);

        }
    }
    
    Sint16 ROINumber;
    bool checkIsExist;
    if (roiName2ROINumber[ROIName]){
        checkIsExist = true;
        ROINumber = roiName2ROINumber[ROIName];
    } else {
        checkIsExist = false;
        ROINumber = roiName2ROINumber.size();
        roiName2ROINumber[ROIName] = ROINumber;
    }
    if (add2RTFile(ROIName, ROINumber, checkIsExist, UID, cData) == -1) exit(0);

    return 1;
}

/* python source code
def load_ben_color(path, sigmaX=10 ):
    image = cv2.imread(path)
    image = cv2.cvtColor(image, cv2.COLOR_BGR2RGB)
    # image = crop_image_from_gray(image)
    # image = cv2.resize(image, (IMG_SIZE, IMG_SIZE))
    image=cv2.addWeighted ( image,4, cv2.GaussianBlur( image , (0,0) , sigmaX) ,-4 ,128)
        
    return image

cv::Mat loadBenColor(std::string path, int sigmaX = 10){
    cv::Mat image = cv::imread(path.c_str());
    cv::cvtColor(image, image, cv::COLOR_BGR2RGB);
    cv::Mat gb, result;
    cv::GaussianBlur(image, gb, cv::Size(0, 0), sigmaX );
    cv::addWeighted(image, 4, gb, -4, 128, result);
    return result;
}
*/
int loadContour(){
    
    struct dirent *de;  // Pointer for directory entry 
    OFString path = "../Data/";
    // opendir() returns a pointer of DIR type.  
    DIR *dr = opendir(path.c_str()); 
    
    if (dr == NULL)  // opendir returns NULL if couldn't open directory 
    { 
        printf("Could not open current directory" ); 
        return 0; 
    } 
    struct dirent *dpatient;
    // Refer http://pubs.opengroup.org/onlinepubs/7990989775/xsh/readdir.html 
    // for readdir() 

    while((dpatient = readdir(dr)) != NULL){
        OFString patientName = dpatient->d_name;
        if (patientName == "." || patientName == "..") continue; // if not folder patient
        DIR *dpm = opendir((path  + patientName + "/masks/").c_str());
        while ((de = readdir(dpm)) != NULL){
            OFString ROIName = de->d_name;
            if (ROIName == "." || ROIName == ".."){
                // printf("%s\n", ROIName.c_str()); 
                continue;
            }
            
            DIR *dirMasks = opendir((path + patientName + "/masks/"  + ROIName + "/").c_str()); //
            if (dirMasks == NULL) continue; // if not folder

            struct dirent *masks;
            while (masks = readdir(dirMasks))
            {
                OFString fileName = masks->d_name;
                if (fileName == "." || fileName == "..") continue; // if not image
                
                cv::Mat image = cv::imread((path + patientName + "/masks/"  + ROIName + "/" + fileName).c_str(), 0);
                // cv::imshow(fileName.c_str(), image); //Debug
                
                std::vector< std::vector< cv::Point > > contourData;
                cv::findContours(image, contourData, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

                // //Debug
                // cv::Mat imageContour(image.rows, image.cols, CV_8UC1, cv::Scalar(0));
                // cv::drawContours(imageContour, contourData, 0, cv::Scalar(255), 1);
                // cv::imshow("res", imageContour);
                // cv::waitKey(0);

                //get file name
                fileName.erase(fileName.size()-4, 4);
                // std::cout << fileName << "\n";
                
                writeToDataset(ROIName, fileName, contourData);

                
            }
        }
    }
    closedir(dr);
    return 1;
}

void gen_random(std::string &s, const int len) {
    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";

    for (int i = 0; i < len; ++i) {
        s[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
    }

    s[len] = 0;
}

bool initRTFile(){
    DcmDataset *dataset = rtFileFormat.getDataset();
    DIR *dr;
    dr = opendir("../imageDCM");
    OFString filename;
    struct dirent *de;
    std::string s = "............................................";
    OFString val;
    
    while ((de = readdir(dr)) != NULL)
    {
        /* code */
        filename = de->d_name;
        if (filename != "." && filename != "..") break;
    }
    
    DcmFileFormat fileinfofm;
    OFCondition cond = fileinfofm.loadFile("../imageDCM/" + filename);
    DcmDataset *fileinfo = fileinfofm.getDataset();
    // std::cout << "../imageDCM/" + filename << "\n";
    
    if (cond.bad()){
        std::cerr << "../imageDCM/" + filename << "\n";
        return false;
    }
    //Patient
    dataset->insertEmptyElement(DCM_PatientName);
    dataset->insertEmptyElement(DCM_PatientID);
    dataset->insertEmptyElement(DCM_PatientBirthDate);
    dataset->insertEmptyElement(DCM_PatientSex);
    dataset->insertEmptyElement(DCM_PatientAddress);
    dataset->insertEmptyElement(DCM_PatientTelephoneNumbers);

    // std::cout << "hi\n";
    
    fileinfo->findAndGetOFString(DCM_ClinicalTrialSponsorName, val);
    dataset->putAndInsertOFStringArray(DCM_ClinicalTrialSponsorName, val);
    fileinfo->findAndGetOFString(DCM_ClinicalTrialProtocolID, val);
    dataset->putAndInsertOFStringArray(DCM_ClinicalTrialProtocolID, val);



    //SOP Common
    gen_random(s, 25);
    // fileinfo->findAndGetOFString(DCM_StudyInstanceUID,val);
    dataset->putAndInsertOFStringArray(DCM_SOPInstanceUID, s.c_str());
    s = "............................................";
    gen_random(s, 25);
    dataset->putAndInsertOFStringArray(DCM_SOPClassUID, s.c_str());
    // std::cout << s << "\n";

    //general study
    dataset->insertEmptyElement(DCM_StudyID);
    dataset->insertEmptyElement(DCM_StudyTime);
    dataset->insertEmptyElement(DCM_StudyDate);
    dataset->insertEmptyElement(DCM_AccessionNumber);
    dataset->insertEmptyElement(DCM_ReferringPhysicianName);

    //RT Series
    dataset->insertEmptyElement(DCM_SeriesNumber);
    dataset->putAndInsertOFStringArray(DCM_Modality, "RTSTRUCT");
    // fileinfo->findAndGetOFString(DCM_SeriesInstanceUID, val);
    s = "............................................";
    gen_random(s, 25);
    dataset->putAndInsertOFStringArray(DCM_SeriesInstanceUID, s.c_str());

    fileinfo->findAndGetOFString(DCM_FrameOfReferenceUID, val);
    dataset->putAndInsertOFStringArray(DCM_FrameOfReferenceUID, val);

    //general equipment
    dataset->insertEmptyElement(DCM_Manufacturer);

    //Structure set
    s = ".....";
    gen_random(s, 5);
    dataset->putAndInsertOFStringArray(DCM_StructureSetLabel, s.c_str());
    dataset->insertEmptyElement(DCM_StructureSetDate);
    dataset->insertEmptyElement(DCM_StructureSetTime);
    s = ".............";
    gen_random(s, 10);
    dataset->putAndInsertOFStringArray(DCM_StructureSetLabel, s.c_str(), true);
    

    return true;
}


int main(int argc, char const *argv[])
{
    if (!initRTFile()){
        std::cout << "init error!!\n";
        return 0; 
    }
    loadContour();
    
    OFCondition cond = rtFileFormat.saveFile("../RT.dcm", EXS_LittleEndianImplicit);
    // std::cout << cond.text() << "\n";
    // // to json
    // std::ofstream out;
    // out.open("../RT.json");
    // rtFileFormat.writeJson(out, DcmJsonFormatPretty(true));
    // out.close();
    std::cout << "done!";
    return 0;
}

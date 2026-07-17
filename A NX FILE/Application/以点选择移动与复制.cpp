#include "以点选择移动与复制.hpp"
using namespace NXOpen;
using namespace NXOpen::BlockStyler;
Session*(以点选择移动与复制::theSession)=NULL; UI*(以点选择移动与复制::theUI)=NULL;
以点选择移动与复制::以点选择移动与复制(){ try{
theSession=NXOpen::Session::GetSession(); theUI=UI::GetUI();
isCopyMode=false; startDispPoint=nullptr; endDispPoint=nullptr;
theDlxFileName="以点选择移动与复制.dlx";
theDialog=theUI->CreateDialog(theDlxFileName);
theDialog->AddApplyHandler(make_callback(this,&以点选择移动与复制::apply_cb));
theDialog->AddOkHandler(make_callback(this,&以点选择移动与复制::ok_cb));
theDialog->AddUpdateHandler(make_callback(this,&以点选择移动与复制::update_cb));
theDialog->AddInitializeHandler(make_callback(this,&以点选择移动与复制::initialize_cb));
theDialog->AddDialogShownHandler(make_callback(this,&以点选择移动与复制::dialogShown_cb));
}catch(exception& ex){throw;} }
以点选择移动与复制::~以点选择移动与复制(){ ClearPoints(); if(theDialog!=NULL){delete theDialog;theDialog=NULL;} }
extern"C" DllExport void ufusr(char*param,int*retcod,int param_len){
以点选择移动与复制* p=NULL; try{p=new 以点选择移动与复制();p->Launch();}catch(exception& ex){
以点选择移动与复制::theUI->NXMessageBox()->Show("Block Styler",NXOpen::NXMessageBox::DialogTypeError,ex.what());}
if(p!=NULL){delete p;p=NULL;} }
extern"C" DllExport int ufusr_ask_unload(){return(int)Session::LibraryUnloadOptionImmediately;}
extern"C" DllExport void ufusr_cleanup(void){try{}catch(exception& ex){以点选择移动与复制::theUI->NXMessageBox()->Show("Block Styler",NXOpen::NXMessageBox::DialogTypeError,ex.what());}}
NXOpen::BlockStyler::BlockDialog::DialogResponse 以点选择移动与复制::Launch(){
NXOpen::BlockStyler::BlockDialog::DialogResponse r=NXOpen::BlockStyler::BlockDialog::DialogResponse::DialogResponseInvalid;
try{r=theDialog->Launch();}catch(exception& ex){theUI->NXMessageBox()->Show("Block Styler",NXOpen::NXMessageBox::DialogTypeError,ex.what());} return r;}
void 以点选择移动与复制::initialize_cb(){ try{
group0=dynamic_cast<NXOpen::BlockStyler::Group*>(theDialog->TopBlock()->FindBlock("group0"));
moveButton=dynamic_cast<NXOpen::BlockStyler::Button*>(theDialog->TopBlock()->FindBlock("moveButton"));
copyButton=dynamic_cast<NXOpen::BlockStyler::Button*>(theDialog->TopBlock()->FindBlock("copyButton"));
group=dynamic_cast<NXOpen::BlockStyler::Group*>(theDialog->TopBlock()->FindBlock("group"));
selectObject=dynamic_cast<NXOpen::BlockStyler::SelectObject*>(theDialog->TopBlock()->FindBlock("selectObject"));
group1=dynamic_cast<NXOpen::BlockStyler::Group*>(theDialog->TopBlock()->FindBlock("group1"));
startPoint=dynamic_cast<NXOpen::BlockStyler::SpecifyPoint*>(theDialog->TopBlock()->FindBlock("startPoint"));
startEnum=dynamic_cast<NXOpen::BlockStyler::Enumeration*>(theDialog->TopBlock()->FindBlock("startEnum"));
group2=dynamic_cast<NXOpen::BlockStyler::Group*>(theDialog->TopBlock()->FindBlock("group2"));
endPoint=dynamic_cast<NXOpen::BlockStyler::SpecifyPoint*>(theDialog->TopBlock()->FindBlock("endPoint"));
endEnum=dynamic_cast<NXOpen::BlockStyler::Enumeration*>(theDialog->TopBlock()->FindBlock("endEnum"));
try{startPoint->SetStepStatusAsString("Optional");}catch(...){}
try{endPoint->SetStepStatusAsString("Optional");}catch(...){}
}catch(exception& ex){theUI->NXMessageBox()->Show("Block Styler",NXOpen::NXMessageBox::DialogTypeError,ex.what());}}
void 以点选择移动与复制::dialogShown_cb(){try{}catch(exception& ex){theUI->NXMessageBox()->Show("Block Styler",NXOpen::NXMessageBox::DialogTypeError,ex.what());}}
int 以点选择移动与复制::update_cb(UIBlock* block){ try{
if(block==moveButton)isCopyMode=false; else if(block==copyButton)isCopyMode=true;
}catch(exception& ex){theUI->NXMessageBox()->Show("Block Styler",NXOpen::NXMessageBox::DialogTypeError,ex.what());} return 0;}
NXOpen::Point3d 以点选择移动与复制::GetBodyCenter(Body* body, double& zmin, double& zmax)
{
    Point3d result(0,0,0); zmin=1e100; zmax=-1e100;
    if (!body) return result;
    // Get bounding box extents from edge vertices (for zmin/zmax)
    std::vector<Edge*> edges = body->GetEdges();
    double xmin=1e100,xmax=-1e100,ymin=1e100,ymax=-1e100;
    for (auto ed : edges) {
        Point3d v1,v2; ed->GetVertices(&v1,&v2);
        if(v1.X<xmin)xmin=v1.X;if(v1.X>xmax)xmax=v1.X;if(v1.Y<ymin)ymin=v1.Y;if(v1.Y>ymax)ymax=v1.Y;if(v1.Z<zmin)zmin=v1.Z;if(v1.Z>zmax)zmax=v1.Z;
        if(v2.X<xmin)xmin=v2.X;if(v2.X>xmax)xmax=v2.X;if(v2.Y<ymin)ymin=v2.Y;if(v2.Y>ymax)ymax=v2.Y;if(v2.Z<zmin)zmin=v2.Z;if(v2.Z>zmax)zmax=v2.Z;
    }
    // Get accurate centroid via MeasureBodies (center of mass)
    try {
        Part* wp = theSession->Parts()->Work();
        MeasureBodyBuilder* b = wp->MeasureManager()->CreateMeasureBodyBuilder(body);
        b->BodyObjects()->Add(body);
        NXOpen::NXObject* obj = b->Commit();
        MeasureBodies* mb = dynamic_cast<MeasureBodies*>(obj);
        if (mb) { result = mb->Centroid(); }
        b->Destroy();
    } catch(...) { }
    return result;
}
try{PropertyList* p=GetBlockProperties("startPoint");sp2=p->GetPoint("Point");sc=true;delete p;}catch(...){}
try{PropertyList* p=GetBlockProperties("endPoint");ep2=p->GetPoint("Point");ec2=true;delete p;}catch(...){}
if(!sc){ if(sv>=1&&sv<=3&&fb){double z=0,z2=0;sp2=GetBodyCenter(fb,z,z2);if(sv==1)sp2.Z=z2;else if(sv==2)sp2.Z=z;}
else{theUI->NXMessageBox()->Show("提示",NXOpen::NXMessageBox::DialogTypeWarning,"请指定起始点");return 1;}}
if(!ec2){ if(ev==1){} else if(ev==2){Point3d o=wp->WCS()->Origin();ep2=o;}
else if(ev==3&&fb){double z=0,z2=0;ep2=GetBodyCenter(fb,z,z2);}
else{theUI->NXMessageBox()->Show("提示",NXOpen::NXMessageBox::DialogTypeWarning,"请指定结束点");return 1;}}
ClearPoints(); double dx=ep2.X-sp2.X,dy=ep2.Y-sp2.Y,dz=ep2.Z-sp2.Z;
theSession->SetUndoMark(NXOpen::Session::MarkVisibilityVisible,isCopyMode?"复制对象":"移动对象");
for(auto obj:objs){Body* b=dynamic_cast<Body*>(obj);if(!b)continue;
Features::MoveObjectBuilder* mb=wp->BaseFeatures()->CreateMoveObjectBuilder(nullptr);
mb->ObjectToMoveObject()->Add(b);
mb->SetMoveObjectResult(isCopyMode?Features::MoveObjectBuilder::MoveObjectResultOptionsCopyOriginal
:Features::MoveObjectBuilder::MoveObjectResultOptionsMoveOriginal);
auto mt=mb->TransformMotion(); mt->SetOption(NXOpen::GeometricUtilities::ModlMotion::OptionsDeltaXyz);
mt->DeltaXc()->SetRightHandSide(std::to_string(dx).c_str());
mt->DeltaYc()->SetRightHandSide(std::to_string(dy).c_str());
mt->DeltaZc()->SetRightHandSide(std::to_string(dz).c_str());
mb->Commit(); mb->Destroy();}
theUI->NXMessageBox()->Show("提示",NXOpen::NXMessageBox::DialogTypeInformation,isCopyMode?"复制完成":"移动完成");
}catch(exception& ex){ec=1;theUI->NXMessageBox()->Show("Block Styler",NXOpen::NXMessageBox::DialogTypeError,ex.what());} return ec;}
int 以点选择移动与复制::ok_cb(){return apply_cb();}
PropertyList* 以点选择移动与复制::GetBlockProperties(const char* b){return theDialog->GetBlockProperties(b);}

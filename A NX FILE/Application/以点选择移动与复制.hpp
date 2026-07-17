#ifndef 以点选择移动与复制_H_INCLUDED
#define 以点选择移动与复制_H_INCLUDED
#include <uf_defs.h>
#include <uf_ui_types.h>
#include <iostream>
#include <NXOpen/Session.hxx>
#include <NXOpen/UI.hxx>
#include <NXOpen/NXMessageBox.hxx>
#include <NXOpen/Callback.hxx>
#include <NXOpen/NXException.hxx>
#include <NXOpen/BlockStyler_UIBlock.hxx>
#include <NXOpen/BlockStyler_BlockDialog.hxx>
#include <NXOpen/BlockStyler_PropertyList.hxx>
#include <NXOpen/BlockStyler_Group.hxx>
#include <NXOpen/BlockStyler_Button.hxx>
#include <NXOpen/BlockStyler_SelectObject.hxx>
#include <NXOpen/BlockStyler_SpecifyPoint.hxx>
#include <NXOpen/BlockStyler_Enumeration.hxx>
#include <NXOpen/Part.hxx>
#include <NXOpen/Body.hxx>
#include <NXOpen/WCS.hxx>
#include <NXOpen/Edge.hxx>
#include <NXOpen/Features_MoveObjectBuilder.hxx>
#include <NXOpen/GeometricUtilities_ModlMotion.hxx>
#include <NXOpen/PartCollection.hxx>
#include <NXOpen/Features_BaseFeatureCollection.hxx>
#include <NXOpen/Point.hxx>
#include <NXOpen/NXObject.hxx>
#include <NXOpen/SelectObjectList.hxx>
#include <NXOpen/SelectNXObjectList.hxx>
#include <NXOpen/MeasureBodies.hxx>
#include <NXOpen/MeasureBodyBuilder.hxx>
#include <cmath>
using namespace std; using namespace NXOpen; using namespace NXOpen::BlockStyler;
class DllExport 以点选择移动与复制 {
public:
    static Session *theSession; static UI *theUI;
    以点选择移动与复制(); ~以点选择移动与复制();
    NXOpen::BlockStyler::BlockDialog::DialogResponse Launch();
    void initialize_cb(); void dialogShown_cb();
    int apply_cb(); int ok_cb(); int update_cb(NXOpen::BlockStyler::UIBlock* block);
    PropertyList* GetBlockProperties(const char *blockID);
private:
    const char* theDlxFileName;
    NXOpen::BlockStyler::BlockDialog* theDialog;
    NXOpen::BlockStyler::Group* group0; NXOpen::BlockStyler::Button* moveButton;
    NXOpen::BlockStyler::Button* copyButton; NXOpen::BlockStyler::Group* group;
    NXOpen::BlockStyler::SelectObject* selectObject; NXOpen::BlockStyler::Group* group1;
    NXOpen::BlockStyler::SpecifyPoint* startPoint; NXOpen::BlockStyler::Enumeration* startEnum;
    NXOpen::BlockStyler::Group* group2; NXOpen::BlockStyler::SpecifyPoint* endPoint;
    NXOpen::BlockStyler::Enumeration* endEnum;
    bool isCopyMode; NXOpen::Point* startDispPoint; NXOpen::Point* endDispPoint;
    NXOpen::Point3d GetBodyCenter(NXOpen::Body* body, double& zmin, double& zmax);
    void ShowPoint(bool isStart, NXOpen::Point3d coord); void ClearPoints();
};
#endif

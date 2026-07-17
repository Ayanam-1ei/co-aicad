#include "以点选择移动与复制.hpp"
#include <sstream>
#include <stdexcept>
#include <uf_csys.h>
#include <vector>

static tag_t resolveBodyTag(NXOpen::TaggedObject* obj)
{
    if (!obj) return NULL_TAG;
    if (auto body = dynamic_cast<NXOpen::Body*>(obj)) return body->Tag();
    if (auto face = dynamic_cast<NXOpen::Face*>(obj)) {
        if (auto b = face->GetBody()) return b->Tag();
        return face->Tag();
    }
    if (auto edge = dynamic_cast<NXOpen::Edge*>(obj)) {
        if (auto b = edge->GetBody()) return b->Tag();
        return edge->Tag();
    }
    return obj->Tag();
}

using namespace NXOpen;
using namespace NXOpen::BlockStyler;

Session*(以点选择移动与复制::theSession) = NULL;
UI*(以点选择移动与复制::theUI) = NULL;

// ---- UFUN 辅助函数 ----
namespace {
    struct DllUfCall {
        DllUfCall()  { UF_initialize(); }
        ~DllUfCall() { UF_terminate(); }
    };
}

// ---- 构造 / 析构 ----
以点选择移动与复制::以点选择移动与复制() {
    try {
        theSession = NXOpen::Session::GetSession();
        theUI = UI::GetUI();
        isCopyMode = false;
        theDlxFileName = "以点选择移动与复制.dlx";
        theDialog = theUI->CreateDialog(theDlxFileName);
        theDialog->AddApplyHandler(make_callback(this, &以点选择移动与复制::apply_cb));
        theDialog->AddOkHandler(make_callback(this, &以点选择移动与复制::ok_cb));
        theDialog->AddUpdateHandler(make_callback(this, &以点选择移动与复制::update_cb));
        theDialog->AddInitializeHandler(make_callback(this, &以点选择移动与复制::initialize_cb));
        theDialog->AddDialogShownHandler(make_callback(this, &以点选择移动与复制::dialogShown_cb));
    } catch (exception&) { throw; }
}

以点选择移动与复制::~以点选择移动与复制() {
    if (theDialog != NULL) { delete theDialog; theDialog = NULL; }
}

// ---- DLL 入口 ----
extern "C" DllExport void ufusr(char* param, int* retcod, int param_len) {
    以点选择移动与复制* p = NULL;
    try {
        p = new 以点选择移动与复制(); p->Launch();
    } catch (exception& ex) {
        以点选择移动与复制::theUI->NXMessageBox()->Show("Block Styler", NXOpen::NXMessageBox::DialogTypeError, ex.what());
    }
    if (p) { delete p; p = NULL; }
}

extern "C" DllExport int ufusr_ask_unload() { return (int)Session::LibraryUnloadOptionImmediately; }
extern "C" DllExport void ufusr_cleanup(void) { try { } catch (exception& ex) {
    以点选择移动与复制::theUI->NXMessageBox()->Show("Block Styler", NXOpen::NXMessageBox::DialogTypeError, ex.what());
}}

NXOpen::BlockStyler::BlockDialog::DialogResponse 以点选择移动与复制::Launch() {
    auto r = NXOpen::BlockStyler::BlockDialog::DialogResponse::DialogResponseInvalid;
    try { r = theDialog->Launch(); } catch (exception& ex) {
        theUI->NXMessageBox()->Show("Block Styler", NXOpen::NXMessageBox::DialogTypeError, ex.what());
    } return r;
}

// ---- 初始化 ----
void 以点选择移动与复制::initialize_cb() {
    try {
        group0 = dynamic_cast<NXOpen::BlockStyler::Group*>(theDialog->TopBlock()->FindBlock("group0"));
        moveButton = dynamic_cast<NXOpen::BlockStyler::Button*>(theDialog->TopBlock()->FindBlock("moveButton"));
        copyButton = dynamic_cast<NXOpen::BlockStyler::Button*>(theDialog->TopBlock()->FindBlock("copyButton"));
        group = dynamic_cast<NXOpen::BlockStyler::Group*>(theDialog->TopBlock()->FindBlock("group"));
        selectObject = dynamic_cast<NXOpen::BlockStyler::SelectObject*>(theDialog->TopBlock()->FindBlock("selectObject"));
        group1 = dynamic_cast<NXOpen::BlockStyler::Group*>(theDialog->TopBlock()->FindBlock("group1"));
        startPoint = dynamic_cast<NXOpen::BlockStyler::SpecifyPoint*>(theDialog->TopBlock()->FindBlock("startPoint"));
        startEnum = dynamic_cast<NXOpen::BlockStyler::Enumeration*>(theDialog->TopBlock()->FindBlock("startEnum"));
        group2 = dynamic_cast<NXOpen::BlockStyler::Group*>(theDialog->TopBlock()->FindBlock("group2"));
        endPoint = dynamic_cast<NXOpen::BlockStyler::SpecifyPoint*>(theDialog->TopBlock()->FindBlock("endPoint"));
        endEnum = dynamic_cast<NXOpen::BlockStyler::Enumeration*>(theDialog->TopBlock()->FindBlock("endEnum"));
        try { startPoint->SetStepStatusAsString("Optional"); } catch (...) {}
        try { endPoint->SetStepStatusAsString("Optional"); } catch (...) {}
    } catch (exception& ex) {
        theUI->NXMessageBox()->Show("Block Styler", NXOpen::NXMessageBox::DialogTypeError, ex.what());
    }
}

void 以点选择移动与复制::dialogShown_cb() { try {} catch (exception& ex) {
    theUI->NXMessageBox()->Show("Block Styler", NXOpen::NXMessageBox::DialogTypeError, ex.what());
}}

int 以点选择移动与复制::update_cb(UIBlock* block) {
    try {
        if (block == moveButton) isCopyMode = false;
        else if (block == copyButton) isCopyMode = true;
    } catch (exception& ex) {
        theUI->NXMessageBox()->Show("Block Styler", NXOpen::NXMessageBox::DialogTypeError, ex.what());
    } return 0;
}

// ---- 应用按钮 ----
int 以点选择移动与复制::apply_cb() {
    int ec = 0;
    Session::UndoMarkId undoMark = theSession->SetUndoMark(
        Session::MarkVisibilityVisible, isCopyMode ? "复制对象" : "移动对象");
    try {
        Part* wp = theSession->Parts()->Work();
        // 获取选中对象的 tags
        std::vector<tag_t> selTags; for (auto obj : selectObject->GetSelectedObjects()) { if (obj) selTags.push_back(obj->Tag()); }
        if (selTags.empty()) {
            theUI->NXMessageBox()->Show("提示", NXOpen::NXMessageBox::DialogTypeWarning, "请选择要操作的对象");
            return 1;
        }

        // 获取第一个对象的 tag 用于体中心计算
        tag_t firstTag = NULL_TAG;
        for (auto obj : selectObject->GetSelectedObjects()) {
            firstTag = resolveBodyTag(obj);
            if (firstTag != NULL_TAG) break;
        }
        Point3d startPt(0,0,0), endPt(0,0,0);
        bool startSpecified=false, endSpecified=false;

        // 尝试读取自由点
        try {
            PropertyList* p = theDialog->GetBlockProperties("startPoint");
            startPt = p->GetPoint("Point");
            if (fabs(startPt.X) > 0.0001 || fabs(startPt.Y) > 0.0001 || fabs(startPt.Z) > 0.0001)
                startSpecified = true;
            delete p;
        } catch (...) {}
        try {
            PropertyList* p = theDialog->GetBlockProperties("endPoint");
            endPt = p->GetPoint("Point");
            if (fabs(endPt.X) > 0.0001 || fabs(endPt.Y) > 0.0001 || fabs(endPt.Z) > 0.0001)
                endSpecified = true;
            delete p;
        } catch (...) {}

        // 读取枚举值
        int sv=3, ev=1;
        try { PropertyList* ep = startEnum->GetProperties(); sv = ep->GetInteger("Value"); delete ep; } catch(...){}
        try { PropertyList* ep = endEnum->GetProperties();   ev = ep->GetInteger("Value"); delete ep; } catch(...){}

        // 计算起始点
        if (!startSpecified) {
            
            double box[6]={0};
            if (UF_MODL_ask_bounding_box(firstTag, box) == 0) {
                double cx=(box[0]+box[3])*0.5, cy=(box[1]+box[4])*0.5;
                if (sv == 1)      startPt = Point3d(cx, cy, box[5]); // 体中心上
                else if (sv == 2) startPt = Point3d(cx, cy, box[2]); // 体中心下
                else              startPt = Point3d(cx, cy, (box[2]+box[5])*0.5); // 体中心
            }
        }

        // 计算结束点
        if (!endSpecified) {
            if (ev == 1)      endPt = Point3d(0,0,0); // ABS原点
            else if (ev == 2) {
                tag_t wcsTag = NULL_TAG;
                double wcsOrg[3] = {0,0,0};
                if (UF_CSYS_ask_wcs(&wcsTag) == 0)
                    UF_CSYS_ask_csys_info(wcsTag, NULL, wcsOrg);
                endPt = Point3d(wcsOrg[0], wcsOrg[1], wcsOrg[2]);
            }
            else if (ev == 3) { // 体中心
                
                double box[6]={0};
                if (UF_MODL_ask_bounding_box(firstTag, box) == 0)
                    endPt = Point3d((box[0]+box[3])*0.5, (box[1]+box[4])*0.5, (box[2]+box[5])*0.5);
            }
        }

        // 计算偏移
        double dx = endPt.X - startPt.X;
        double dy = endPt.Y - startPt.Y;
        double dz = endPt.Z - startPt.Z;

        if (fabs(dx) < 1e-9 && fabs(dy) < 1e-9 && fabs(dz) < 1e-9) {
            theUI->NXMessageBox()->Show("提示", NXOpen::NXMessageBox::DialogTypeInformation,
                "起始点与结束点相同，无需操作");
             return 0;
        }

        // 执行移动/复制
        {
            Matrix4x4 xformMat;
            xformMat.Rxx = 1.0; xformMat.Rxy = 0.0; xformMat.Rxz = 0.0; xformMat.Xt = dx;
            xformMat.Ryx = 0.0; xformMat.Ryy = 1.0; xformMat.Ryz = 0.0; xformMat.Yt = dy;
            xformMat.Rzx = 0.0; xformMat.Rzy = 0.0; xformMat.Rzz = 1.0; xformMat.Zt = dz;

            Features::MoveObjectBuilder* moveBuilder = wp->BaseFeatures()->CreateMoveObjectBuilder(NULL);
            for (auto obj : selectObject->GetSelectedObjects())
                moveBuilder->ObjectToMoveObject()->Add(dynamic_cast<NXObject*>(obj));

            moveBuilder->SetMoveObjectResult(isCopyMode ?
                Features::MoveObjectBuilder::MoveObjectResultOptionsCopyOriginal :
                Features::MoveObjectBuilder::MoveObjectResultOptionsMoveOriginal);

            moveBuilder->TransformMotion()->SetOption(GeometricUtilities::ModlMotion::OptionsNone);
            moveBuilder->SetPreMultiplicationTransform(xformMat);
            moveBuilder->Commit();
            moveBuilder->Destroy();
        }
        // 更新建模
        if (theSession->UpdateManager()) theSession->UpdateManager()->DoUpdate(undoMark);

        theUI->NXMessageBox()->Show("提示", NXOpen::NXMessageBox::DialogTypeInformation,
            isCopyMode ? "复制完成" : "移动完成");

    } catch (exception& ex) {
        ec = 1;
        theSession->UndoToMark(undoMark, isCopyMode ? "复制对象" : "移动对象");
        theUI->NXMessageBox()->Show("Block Styler", NXOpen::NXMessageBox::DialogTypeError, ex.what());
    }
    return ec;
}

int 以点选择移动与复制::ok_cb() { return apply_cb(); }
PropertyList* 以点选择移动与复制::GetBlockProperties(const char* b) { return theDialog->GetBlockProperties(b); }

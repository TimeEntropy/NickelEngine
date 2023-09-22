#include "file_dialog.hpp"

std::vector<std::string> OpenFileDialog(const std::string& title) {
    OPENFILENAME ofn;
    char szFile[MAX_PATH] = {0};

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR |
                OFN_ALLOWMULTISELECT | OFN_EXPLORER | OFN_ENABLESIZING;
    ofn.lpstrTitle = title.c_str();

    if (GetOpenFileName(&ofn) == TRUE) {
        return {std::string{szFile}};
    } else {
        return {};
    }
}

std::string OpenDirDialog(const std::string& title) {
    BROWSEINFO bi = {0};
    bi.hwndOwner = NULL;
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    bi.lpszTitle = title.c_str();

    LPITEMIDLIST pidl = SHBrowseForFolder(&bi);
    std::string result;
    if (pidl != NULL) {
        char folderPath[MAX_PATH] = {0};
        SHGetPathFromIDList(pidl, folderPath);
        result = folderPath;

        CoTaskMemFree(pidl);
    }

    return result;
}
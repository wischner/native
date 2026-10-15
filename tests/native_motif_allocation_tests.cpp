//
// Reproduces SDK-only allocation behavior without linking Native.
// Controls isolate text selection, notebook and drop-interest teardown.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <Xm/Xm.h>
#include <Xm/TextF.h>
#include <Xm/Notebook.h>
#include <Xm/Form.h>
#include <cstdlib>
int main(int argc, char **argv) {
    const int scenario = argc > 1 ? std::atoi(argv[1]) : 0;
    XtAppContext app;
    Widget shell = XtVaAppInitialize(&app, "Cleanup", nullptr, 0, &argc, argv,
                                     nullptr, nullptr);
    Widget form =
        XtVaCreateManagedWidget("form", xmFormWidgetClass, shell, nullptr);
    Widget text = XtVaCreateManagedWidget(
        "text", xmTextFieldWidgetClass, form, XmNvalue, "select me", nullptr);
    if (scenario == 2)
        XtVaCreateManagedWidget("book", xmNotebookWidgetClass, form, nullptr);
    XtRealizeWidget(shell);
    if (scenario == 1) {
        XmProcessTraversal(text, XmTRAVERSE_CURRENT);
        XmTextFieldSetSelection(text, 0, 9, CurrentTime);
    }
    XtDestroyWidget(shell);
    XtDestroyApplicationContext(app);
}

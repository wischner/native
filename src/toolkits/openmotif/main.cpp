#include <Xm/Xm.h>
#include <Xm/PushB.h> // Example widget
#include <iostream>

// User program() declaration
extern int program(int argc, char **argv);

XtAppContext app_context;
Widget top_level_shell = nullptr;

void quit_callback(Widget widget, XtPointer client_data, XtPointer call_data) {
    std::cout << "Quit callback triggered." << std::endl;
    XtDestroyApplicationContext(app_context);
    exit(0);
}

int main(int argc, char **argv) {
    std::cout << "Starting OpenMotif toolkit main()" << std::endl;

    // Initialize Xt and create the application context
    top_level_shell = XtVaAppInitialize(
        &app_context,
        "NativeOpenMotifApp",
        nullptr,
        0,
        &argc,
        argv,
        nullptr,
        nullptr
    );

    if (!top_level_shell) {
        std::cerr << "Failed to create top-level shell." << std::endl;
        return 1;
    }

    std::cout << "Top-level shell created successfully." << std::endl;

    // Call the user's program()
    int result = program(argc, argv);
    std::cout << "User program() returned " << result << std::endl;

    // Create a simple widget (PushButton example)
    Widget button = XtVaCreateManagedWidget(
        "QuitButton",
        xmPushButtonWidgetClass,
        top_level_shell,
        XmNwidth, 200,
        XmNheight, 50,
        nullptr
    );

    XtAddCallback(button, XmNactivateCallback, quit_callback, nullptr);

    // Realize the widget hierarchy
    XtRealizeWidget(top_level_shell);

    std::cout << "Entering XtAppMainLoop..." << std::endl;

    // Start the event loop
    XtAppMainLoop(app_context);

    return result;
}

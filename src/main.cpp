#include "../include/UI_Interface.h"
#include <gtkmm.h>

int main(int argc, char *argv[])
{
    auto app = Gtk::Application::create(argc, argv);

    UI_Interface win;

    return app->run(win);

}

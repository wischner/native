#include <cstdint>

#include "nice.hpp"

using namespace nice;

// Linked resource: Tutankhamun raw BGRA raster.
#define TUT_WIDTH   256
#define TUT_HEIGHT  192 
extern uint8_t tut_raster[];

#define WIN_WIDTH   1024
#define WIN_HEIGHT  512

// Main window.
class main_wnd : public app_wnd {
public:
    main_wnd() : app_wnd("Raster", { WIN_WIDTH,WIN_HEIGHT })
    {
        paint.connect(this, &main_wnd::on_paint);
    }
private:
    // Raster class, converts BGRA to native format.
    raster tut_{TUT_WIDTH,TUT_HEIGHT,tut_raster};

    bool on_paint(const artist& a) {
        rct client=paint_area;
        a.fill_rect({ 0,0,0 }, client);
        a.draw_raster(tut_, 
            { client.w/2-TUT_WIDTH/2,
              client.h/2-TUT_HEIGHT/2
            }
        );
        return true;
    }
};

void program()
{
    app::run(main_wnd());
}
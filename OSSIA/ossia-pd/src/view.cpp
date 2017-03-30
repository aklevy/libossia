// A starter for Pd objects
#include "view.hpp"
#include "remote.hpp"

namespace ossia { namespace pd {

static t_eclass *view_class;

//****************//
// Member methods //
//****************//
bool t_view :: register_node(ossia::net::node_base*  node){
    bool res = do_registration(node);
    if (res) {
        obj_dequarantining<t_view>(this);
        std::vector<obj_hierachy> viewnode = find_child_to_register(this, x_obj.o_canvas->gl_list, "ossia.view");
        for (auto v : viewnode){
            if(v.classname == "ossia.view"){
                t_view* view = (t_view*) v.x;
                if (view == this) {
                    // not registering itself
                    continue;
                }
                view->register_node(x_node);
            } else if(v.classname == "ossia.remote"){
                t_remote* remote = (t_remote*) v.x;
                remote->register_node(x_node);
            }
        }
    } else obj_quarantining<t_view>(this);

    return res;
}

bool t_view :: do_registration(ossia::net::node_base*  node){
    if (x_node && x_node->getParent() == node ) return true; // already register to this node;
    unregister(); // we should unregister here because we may have add a node between the registered node and the remote

    if (node){
        std::string absolute_path = get_absolute_path<t_view>(this);
        std::string address_string = ossia::net::address_string_from_node(*node);

        if ( absolute_path != address_string) return false;
        x_node = node->findChild(x_name->s_name);
        if (x_node) {
            x_node->aboutToBeDeleted.connect<t_view, &t_view::isDeleted>(this);
        } else {
            return false;
        }
    } else {
        return false;
    }

    return true;
}

static void register_children(t_view* x){
    std::vector<obj_hierachy> viewnode = find_child_to_register(x, x->x_obj.o_canvas->gl_list, "ossia.view");
    for (auto v : viewnode){
        if(v.classname == "ossia.view"){
            t_view* view = (t_view*) v.x;
            if (view == x) continue;
            obj_register<t_view>(view);
        } else if(v.classname == "ossia.remote"){
            t_remote* remote = (t_remote*) v.x;
            obj_register<t_remote>(remote);
        }
    }
}

bool t_view :: unregister(){
    if(!x_node) return true; // not registered

    std::vector<obj_hierachy> viewnode = find_child_to_register(this, x_obj.o_canvas->gl_list, "ossia.view");
    for (auto v : viewnode){
        if(v.classname == "ossia.view"){
            t_view* view = (t_view*) v.x;
            if (view == this) continue;
            view->unregister();
        } else if(v.classname == "ossia.remote"){
            t_remote* remote = (t_remote*) v.x;
            remote->unregister();
        }
    }
    x_node = nullptr;
    obj_quarantining<t_view>(this);

    register_children(this);

    return true;
}

static void view_click(t_view *x,
    t_floatarg xpos, t_floatarg ypos, t_floatarg shift,
    t_floatarg ctrl, t_floatarg alt){

  using namespace std::chrono;
  milliseconds ms = duration_cast< milliseconds >( system_clock::now().time_since_epoch() );
  milliseconds diff = (ms - x->x_last_click);
  if ( diff.count() < 200 ){ // 200 ms double click
    x->x_last_click = milliseconds(0);

    int l;
    t_device *device = (t_device*) find_parent(&x->x_obj,"ossia.device", 0, &l);
    /*
    if (!device || !x->x_node || obj_isQuarantined<t_remote>(x)){
      pd_error(x, "sorry no device found, or not connected or quarantined...");
      return;
    }
    */

    t_canvas *root = x->x_obj.o_canvas;
    while (root->gl_owner) root = root->gl_owner;
    if (!find_and_display_friend(x, root))   pd_error(x,"sorry I can't find a connected friend :-(");
  } else {
    x->x_last_click = ms;
  }
}

static void *view_new(t_symbol *name, int argc, t_atom *argv)
{
    t_view *x = (t_view *)eobj_new(view_class);

    if(x)
    {
        x->x_dumpout = outlet_new((t_object*)x, gensym("dumpout"));
        x->x_clock = clock_new(x, (t_method)obj_tick);
        x->x_regclock = clock_new(x, (t_method)obj_register<t_view>);

        if (argc != 0 && argv[0].a_type == A_SYMBOL) {
            x->x_name = atom_getsymbol(argv);
            if (std::string(x->x_name->s_name) != "" && x->x_name->s_name[0] == '/') x->x_absolute = true;

            // we need to delay registration because object may use patcher hierarchy to check address validity
            // and object will be added to patcher's objects list (aka canvas g_list) after model_new() returns.
            // 0 ms delay means that it will be perform on next clock tick
            clock_delay(x->x_regclock, 0);
        } else {
            x->x_name = gensym("untitledModel");
            pd_error(x,"You have to pass a name as the first argument");
        }
    }

    return (x);
}

static void view_free(t_view *x)
{
    x->x_dead = true;
    x->unregister();
    obj_dequarantining<t_view>(x);
    clock_free(x->x_regclock);
    clock_free(x->x_clock);
}

extern "C" void setup_ossia0x2eview(void)
{
    t_eclass *c = eclass_new("ossia.view", (method)view_new, (method)view_free, (short)sizeof(t_view), CLASS_DEFAULT, A_GIMME, 0);

    if(c)
    {
        eclass_addmethod(c, (method) obj_dump<t_view>,       "dump",       A_NULL, 0);
        eclass_addmethod(c, (method) view_click,    "click",      A_NULL,  0);
    }

    view_class = c;
}
} }

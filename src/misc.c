/* *************************************************************************
 *     misc.c：雜項。
 *     版權 (C) 2020-2022 gsm <406643764@qq.com>
 *     本程序為自由軟件：你可以依據自由軟件基金會所發布的第三版或更高版本的
 * GNU通用公共許可證重新發布、修改本程序。
 *     雖然基于使用目的而發布本程序，但不負任何擔保責任，亦不包含適銷性或特
 * 定目標之適用性的暗示性擔保。詳見GNU通用公共許可證。
 *     你應該已經收到一份附隨此程序的GNU通用公共許可證副本。否則，請參閱
 * <http://www.gnu.org/licenses/>。
 * ************************************************************************/

#include <sys/types.h>
#include <sys/wait.h>
#include "gwm.h"
#include "client.h"
#include "misc.h"

void *malloc_s(size_t size)
{
    void *p=malloc(size);
    if(p == NULL)
        exit_with_msg("錯誤：申請內存失敗");
    return p;
}

void exit_with_perror(const char *s)
{
    perror(s);
    exit(EXIT_FAILURE);
}

void exit_with_msg(const char *msg)
{
    fprintf(stderr, "%s\n", msg);
    exit(EXIT_FAILURE);
}

bool is_wm_win(WM *wm, Window win)
{
    XWindowAttributes attr;
    Atom type=get_atom_prop(wm, win, wm->ewmh_atom[_NET_WM_WINDOW_TYPE]);

    return (XGetWindowAttributes(wm->display, win, &attr)
        && attr.map_state != IsUnmapped
        && !attr.override_redirect
        && ( type == wm->ewmh_atom[_NET_WM_WINDOW_TYPE_UTILITY]
            || type == wm->ewmh_atom[_NET_WM_WINDOW_TYPE_DIALOG]
            || type == wm->ewmh_atom[_NET_WM_WINDOW_TYPE_NORMAL]));
}

/* 在調用XSetWindowBackground之後，在收到下一個顯露事件或調用XClearWindow
 * 之前，背景不變。此處用發送顯露事件的方式使背景設置立即生效。*/
void update_win_background(WM *wm, Window win, unsigned long color)
{
    XEvent event={.xexpose={.type=Expose, .window=win}};
    XSetWindowBackground(wm->display, win, color);
    XSendEvent(wm->display, win, False, NoEventMask, &event);
}

Widget_type get_widget_type(WM *wm, Window win)
{
    Widget_type type;
    Client *c;
    if(win == wm->root_win)
        return ROOT_WIN;
    if(win == wm->run_cmd.win)
        return RUN_CMD_ENTRY;
    for(type=TASKBAR_BUTTON_BEGIN; type<=TASKBAR_BUTTON_END; type++)
        if(win == wm->taskbar.buttons[TASKBAR_BUTTON_INDEX(type)])
            return type;
    if(win == wm->taskbar.status_area)
        return STATUS_AREA;
    for(Client *c=wm->clients->next; c!=wm->clients; c=c->next)
        if(c->area_type==ICONIFY_AREA && win==c->icon->win)
            return CLIENT_ICON;
    for(type=CMD_CENTER_ITEM_BEGIN; type<=CMD_CENTER_ITEM_END; type++)
        if(win == wm->cmd_center.items[CMD_CENTER_ITEM_INDEX(type)])
            return type;
    if((c=win_to_client(wm, win)))
    {
        if(win == c->win)
            return CLIENT_WIN;
        else if(win == c->frame)
            return CLIENT_FRAME;
        else if(win == c->title_area)
            return TITLE_AREA;
        else
            for(type=TITLE_BUTTON_BEGIN; type<=TITLE_BUTTON_END; type++)
                if(win == c->buttons[TITLE_BUTTON_INDEX(type)])
                    return type;
    }
    return UNDEFINED;
}

Pointer_act get_resize_act(Client *c, const Move_info *m)
{   // 窗口邊框寬度、標題欄調試、可調整尺寸區域的寬度、高度
    // 以及窗口框架左、右橫坐標和上、下縱坐標
    int bw=c->border_w, bh=c->title_bar_h, rw=c->w/4, rh=c->h/4,
        lx=c->x-bw, rx=c->x+c->w+bw, ty=c->y-bh-bw, by=c->y+c->h+bw;

    if(m->ox>=lx && m->ox<lx+bw+rw && m->oy>=ty && m->oy<ty+bw+rh)
        return TOP_LEFT_RESIZE;
    else if(m->ox>=rx-bw-rw && m->ox<rx && m->oy>=ty && m->oy<ty+bw+rh)
        return TOP_RIGHT_RESIZE;
    else if(m->ox>=lx && m->ox<lx+bw+rw && m->oy>=by-bw-rh && m->oy<by)
        return BOTTOM_LEFT_RESIZE;
    else if(m->ox>=rx-bw-rw && m->ox<rx && m->oy>=by-bw-rh && m->oy<by)
        return BOTTOM_RIGHT_RESIZE;
    else if(m->oy>=ty && m->oy<ty+bw+rh)
        return TOP_RESIZE;
    else if(m->oy>=by-bw-rh && m->oy<by)
        return BOTTOM_RESIZE;
    else if(m->ox>=lx && m->ox<lx+bw+rw)
        return LEFT_RESIZE;
    else if(m->ox>=rx-bw-rw && m->ox<rx)
        return RIGHT_RESIZE;
    else
        return NO_OP;
}

void clear_zombies(int unused)
{
	while(0 < waitpid(-1, NULL, WNOHANG))
        ;
}

bool is_chosen_button(WM *wm, Widget_type type)
{
    return(type == DESKTOP_BUTTON_BEGIN+wm->cur_desktop-1
        || type == LAYOUT_BUTTON_BEGIN+DESKTOP(wm).cur_layout);
}

void set_xic(WM *wm, Window win, XIC *ic)
{
    if(wm->xim == NULL)
        return;
    if((*ic=XCreateIC(wm->xim, XNInputStyle, XIMPreeditNothing|XIMStatusNothing,
        XNClientWindow, win, NULL)) == NULL)
        fprintf(stderr, "錯誤：窗口（0x%lx）輸入法設置失敗！", win);
    else
        XSetICFocus(*ic);
}

Window get_transient_for(WM *wm, Window w)
{
    Window pw;
    return XGetTransientForHint(wm->display, w, &pw) ? pw : None;
}

KeySym look_up_key(XIC xic, XKeyEvent *e, wchar_t *keyname, size_t n)
{
	KeySym ks;
    if(xic)
        XwcLookupString(xic, e, keyname, n, &ks, 0);
    else
    {
        char kn[n];
        XLookupString(e, kn, n, &ks, 0);
        mbstowcs(keyname, kn, n);
    }
    return ks;
}

Atom get_atom_prop(WM *wm, Window win, Atom prop)
{
	int di;
	unsigned long dl;
	unsigned char *p=NULL;
	Atom da, atom=None;

	if(XGetWindowProperty(wm->display, win, prop, 0L, sizeof(atom), False, XA_ATOM,
		&da, &di, &dl, &dl, &p) == Success && p)
		atom=*(Atom *)p, XFree(p);
	return atom;
}

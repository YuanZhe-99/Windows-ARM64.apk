package gui

import "strings"

// A tab is what a pane shows: Home, Diagnostics or an open app.
const (
	tabHome = "home"
	tabDiag = "diag"
)

// appTab is an open app's tab.
func appTab(id string) string { return "app:" + id }

// appOf is the entry ID an app's tab shows, "" for Home and Diagnostics.
func appOf(tab string) string {
	id, _ := strings.CutPrefix(tab, "app:")
	if id == tab {
		return ""
	}
	return id
}

// node is a node of a group's split (design/README.md, "Groups and
// sizes"): a tab, or two nodes side by side (across) or one above the
// other, the first taking ratio percent of the room; the dividers bind
// ratio.
type node struct {
	tab    string
	across bool
	ratio  float32
	first  *node
	second *node
}

// group is panes shown together: their split, the pane in focus, and the
// one maximized over the others ("" for none).
type group struct {
	root  *node
	focus string
	zoom  string
}

// tabs are the group's tabs in reading order.
func (g *group) tabs() []string {
	var out []string
	var walk func(n *node)
	walk = func(n *node) {
		switch {
		case n == nil:
		case n.first == nil:
			out = append(out, n.tab)
		default:
			walk(n.first)
			walk(n.second)
		}
	}
	walk(g.root)
	return out
}

// single shows t alone.
func (g *group) single(t string) { *g = group{root: &node{tab: t}, focus: t} }

// leaf is the node showing t and its parent (nil for the root).
func (g *group) leaf(t string) (n, parent *node) {
	var find func(n, up *node) (*node, *node)
	find = func(n, up *node) (*node, *node) {
		switch {
		case n == nil:
			return nil, nil
		case n.first == nil:
			if n.tab == t {
				return n, up
			}
			return nil, nil
		}
		if l, u := find(n.first, n); l != nil {
			return l, u
		}
		return find(n.second, n)
	}
	return find(g.root, nil)
}

// split puts t beside target (across) or below it, halving target's pane;
// t takes the focus. Returns: false when target isn't shown or the group
// already shows maxPanes.
func (g *group) split(t, target string, across bool) bool {
	n, _ := g.leaf(target)
	if n == nil || len(g.tabs()) >= maxPanes {
		return false
	}
	n.first, n.second = &node{tab: n.tab}, &node{tab: t}
	n.tab, n.across, n.ratio = "", across, 50
	g.focus, g.zoom = t, ""
	return true
}

// remove takes t's pane out; its sibling takes the room and, when t had
// the focus, the focus. Returns: whether t was shown.
func (g *group) remove(t string) bool {
	n, parent := g.leaf(t)
	if n == nil {
		return false
	}
	if g.zoom == t {
		g.zoom = ""
	}
	if parent == nil {
		*g = group{}
		return true
	}
	sibling := parent.first
	if sibling == n {
		sibling = parent.second
	}
	*parent = *sibling
	if g.focus == t {
		for parent.first != nil {
			parent = parent.first
		}
		g.focus = parent.tab
	}
	return true
}

// size is a width and a height in DIPs.
type size struct{ w, h float32 }

// canSplit tells whether a pane of room r has room for two halves, each
// at least paneMinW × paneMinH, beside each other (across) or one above
// the other.
func canSplit(r size, across bool) bool {
	if across {
		return r.w >= 2*paneMinW+margin
	}
	return r.h >= 2*paneMinH+headerH+margin
}

// groupOf is the group showing t, or nil.
func (a *App) groupOf(t string) *group {
	for _, g := range a.groups {
		if n, _ := g.leaf(t); n != nil {
			return g
		}
	}
	return nil
}

// homeGroup is the group showing Home.
func (a *App) homeGroup() *group { return a.groupOf(tabHome) }

// showTab shows t in the main window: beside the pane in focus where it
// has room, else as a group of its own. A tab shown already comes forward.
func (a *App) showTab(t string, beside bool) {
	if g := a.groupOf(t); g != nil {
		a.cur, g.focus = g, t
		return
	}
	if beside && a.cur != nil && canSplit(a.sizes[a.cur.focus], true) && a.cur.split(t, a.cur.focus, true) {
		return
	}
	g := &group{}
	g.single(t)
	a.groups = append(a.groups, g)
	a.cur = g
}

// splitWith moves t beside target (across) or below it, out of the group
// it showed in.
func (a *App) splitWith(target, t string, across bool) {
	g := a.groupOf(target)
	if g == nil || t == target {
		return
	}
	a.removeTab(t)
	if g.split(t, target, across) {
		a.cur = g
		return
	}
	a.showTab(t, false)
}

// removeTab takes t out of whichever group shows it; an empty group goes,
// and the main window then shows Home's.
func (a *App) removeTab(t string) {
	for i, g := range a.groups {
		if !g.remove(t) {
			continue
		}
		if g.root == nil {
			a.groups = append(a.groups[:i], a.groups[i+1:]...)
			if a.cur == g {
				a.cur = nil
			}
		}
		break
	}
	if a.cur == nil || a.groupOf(a.cur.focus) == nil {
		a.cur = a.homeGroup()
	}
	if a.cur == nil {
		// Home was moved into a group that has gone: it shows alone again.
		g := &group{}
		g.single(tabHome)
		a.groups = append([]*group{g}, a.groups...)
		a.cur = g
	}
}

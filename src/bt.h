// bt.h: a very small behaviour tree (our own; the shape follows BehaviorTree.CPP's selector and sequence nodes).
// A node is anything callable that returns bt::St. Nothing is allocated: the tree is written out where it is run.
//   bt::selector(a, b, c)   runs each in turn until one does not FAIL (a fallback: the first thing that can be done)
//   bt::sequence(a, b, c)   runs each in turn until one does not succeed (all of them, in order)
//   bt::when(cond, node)    FAIL unless cond; then the node
#pragma once
namespace bt {
enum St { FAIL, OK, RUN };
inline St selector() { return FAIL; }
template <class F, class... R> inline St selector(F&& f, R&&... r) { St s = f(); return s != FAIL ? s : selector(static_cast<R&&>(r)...); }
inline St sequence() { return OK; }
template <class F, class... R> inline St sequence(F&& f, R&&... r) { St s = f(); return s != OK ? s : sequence(static_cast<R&&>(r)...); }
template <class F> inline St when(bool c, F&& f) { return c ? f() : FAIL; }
}

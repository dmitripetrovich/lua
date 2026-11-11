print("testing read-only tables")

local RO = "attempting to modify a readonly value"
local ARG = "table is readonly"

local function check (msg, f, ...)
  local ok, err = pcall(f, ...)
  assert(not ok, "write was not rejected")
  assert(string.find(err, msg, 1, true), err)
  return err
end

local function checkvm (f, ...)
  local err = check(RO, f, ...)
  assert(string.find(err, "^[^:]+:%d+: attempting to modify a readonly value$"), err)
end

local function same (t, u)
  for k, v in pairs(t) do assert(u[k] == v) end
  for k, v in pairs(u) do assert(t[k] == v) end
end


do
  local t = {}
  assert(table.isfrozen(t) == false)
  assert(table.freeze(t) == t)
  assert(table.isfrozen(t) == true)
  assert(table.freeze(t) == t)
  assert(table.isfrozen(t) == true)
  check("table expected", table.freeze, 1)
  check("table expected", table.freeze)
  check("table expected", table.isfrozen, "x")
  check("table expected", table.isfrozen, nil)
  local p = setmetatable({}, {__metatable = false})
  check("cannot change a protected metatable", table.freeze, p)
  assert(not table.isfrozen(p))
  p.x = 1
  assert(p.x == 1)
end


do
  local t = table.freeze({10, 20, 30, x = 1, y = "a", [1.5] = true, [false] = 0})
  local orig = {10, 20, 30, x = 1, y = "a", [1.5] = true, [false] = 0}
  local i, k, key = 2, "x", {}

  checkvm(function () t[1] = 0 end)
  checkvm(function () t[3] = nil end)
  checkvm(function () t[4] = 40 end)
  checkvm(function () t[100] = 1 end)
  checkvm(function () t[i] = 0 end)
  checkvm(function () t[i + 2] = 0 end)
  checkvm(function () t[-1] = 0 end)
  checkvm(function () t[0] = 0 end)
  checkvm(function () t[math.mininteger] = 0 end)
  checkvm(function () t[2.0] = 0 end)
  checkvm(function () t[2^53] = 0 end)
  checkvm(function () t[1.5] = false end)
  checkvm(function () t.x = 2 end)
  checkvm(function () t.x = nil end)
  checkvm(function () t.z = 2 end)
  checkvm(function () t[k] = 2 end)
  checkvm(function () t["new" .. k] = 2 end)
  checkvm(function () t[key] = 2 end)
  checkvm(function () t[false] = 1 end)
  checkvm(function () t[true] = 1 end)
  checkvm(function () t[print] = 1 end)
  checkvm(function () t[nil] = 1 end)
  checkvm(function () t[0/0] = 1 end)
  checkvm(function () t.x, t.y = 5, 6 end)
  checkvm(function () local a = t; a[1] = a[1] end)
  same(t, orig)
  assert(#t == 3)
end


do
  local t = table.new(16, 4)
  t.gone = 1
  t.gone = nil
  t[3] = 1
  t[3] = nil
  table.freeze(t)
  for i = 1, 16 do
    checkvm(function () t[i] = i end)
  end
  checkvm(function () t.gone = 1 end)
  checkvm(function () t.fresh = 1 end)
  assert(next(t) == nil)
  assert(#t == 0)
end


do
  local t = table.freeze({1, 2, 3, n = 4, s = "a"})
  local i, k = 2, "n"
  local f = assert(load([[
    local t, i, k, which = ...
    if which == 1 then t[1] += 1
    elseif which == 2 then t[i] -= 1
    elseif which == 3 then t.n *= 2
    elseif which == 4 then t[k] /= 2
    elseif which == 5 then t.n <<= 1
    elseif which == 6 then t[3] |= 8
    elseif which == 7 then t[i] &= 1
    elseif which == 8 then t[i + 10] += 1
    end
  ]]))
  for which = 1, 7 do
    checkvm(f, t, i, k, which)
  end
  check("attempt to perform arithmetic on a nil value", f, t, i, k, 8)
  same(t, {1, 2, 3, n = 4, s = "a"})
end


do
  local env = table.freeze({x = 1, print = print})
  local set = assert(load("x = 2", "=chunk", "t", env))
  local new = assert(load("y = 2", "=chunk", "t", env))
  local get = assert(load("return x, print", "=chunk", "t", env))
  check("chunk:1: " .. RO, set)
  check("chunk:1: " .. RO, new)
  local a, b = get()
  assert(a == 1 and b == print)
  assert(env.x == 1 and rawget(env, "y") == nil)
  local f = assert(load("function g () end", "=chunk", "t", env))
  check(RO, f)
  local g = assert(load("local function h () end; return h", "=chunk", "t", env))
  assert(type(g()) == "function")
end


do
  local ro = table.freeze({})
  local function swap ()
    local n = 1
    while true do
      local name, value = debug.getlocal(2, n)
      if name == nil then break end
      if type(value) == "table" and value ~= ro and next(value) == nil then
        debug.setlocal(2, n, ro)
        return 0
      end
      n = n + 1
    end
    error("no table under construction")
  end
  local function build () return {swap(), 1, 2, 3} end
  local function buildmany () return {swap(), table.unpack({1, 2, 3, 4, 5})} end
  local function buildfield () return {swap(), a = 1} end
  checkvm(build)
  checkvm(buildmany)
  checkvm(buildfield)
  assert(next(ro) == nil)
  local t = {1, 2, 3}
  assert(#t == 3)
end


do
  local t = table.freeze({1, 2, a = 1})
  check(ARG, rawset, t, 1, 0)
  check(ARG, rawset, t, 3, 0)
  check(ARG, rawset, t, "a", nil)
  check(ARG, rawset, t, "b", 1)
  check(ARG, setmetatable, t, {})
  check(ARG, setmetatable, t, nil)
  check(RO, debug.setmetatable, t, {})
  check(RO, debug.setmetatable, t, nil)
  assert(getmetatable(t) == nil)
  same(t, {1, 2, a = 1})
  local w = {}
  assert(rawset(w, 1, 2) == w and w[1] == 2)
  check("bad argument #1 to 'rawset' (table expected, got number)", rawset, 1, 1, 1)
end


do
  local t = table.freeze({5, 3, 1, 4, 2})
  local orig = {5, 3, 1, 4, 2}
  check("bad argument #1 to 'table.insert' (" .. ARG .. ")", table.insert, t, 6)
  check(ARG, table.insert, t, 1, 6)
  check(ARG, table.insert, t, 6, 6)
  check("bad argument #1 to 'table.remove' (" .. ARG .. ")", table.remove, t)
  check(ARG, table.remove, t, 1)
  check(ARG, table.remove, table.freeze({}))
  check("bad argument #1 to 'table.sort' (" .. ARG .. ")", table.sort, t)
  check(ARG, table.sort, t, function (a, b) return a > b end)
  check(ARG, table.sort, table.freeze({}))
  check("bad argument #5 to 'table.move' (" .. ARG .. ")", table.move, {1, 2}, 1, 2, 1, t)
  check("bad argument #1 to 'table.move' (" .. ARG .. ")", table.move, t, 1, 2, 3)
  check(ARG, table.move, t, 1, 0, 1)
  check("bad argument #1 to 'table.", table.wipe, t)
  check(ARG, table.wipe, t)
  check(ARG, table.clear, t)
  check(ARG, table.compact, t)
  check("bad argument #2 to 'table.clone' (" .. ARG .. ")", table.clone, {1}, t)
  same(t, orig)

  local dst = {}
  assert(table.move(t, 1, 5, 1, dst) == dst)
  same(dst, orig)
  assert(table.concat(t, ",") == "5,3,1,4,2")
  assert(select("#", table.unpack(t)) == 5)
  local a, b, c = table.unpack(t, 2, 4)
  assert(a == 3 and b == 1 and c == 4)
  assert(table.clone(t, t) == t)
  assert(table.isfrozen(t))

  local copy = table.clone(t)
  assert(copy ~= t and not table.isfrozen(copy))
  same(copy, orig)
  copy[1] = 0
  table.insert(copy, 9)
  table.sort(copy)
  assert(table.concat(copy, ",") == "0,1,2,3,4,9")
  same(t, orig)

  local sink = {x = 1}
  assert(table.clone(t, sink) == sink)
  assert(not table.isfrozen(sink) and sink.x == nil)
  same(sink, orig)
  sink.x = 1
  assert(table.wipe(sink) == sink and next(sink) == nil)

  local fz = table.freeze(table.clone({1, 2, h = "x"}))
  local fc = table.clone(fz)
  fc.h = "y"
  assert(fz.h == "x" and fc.h == "y")
  assert(table.type(t) == "array" and table.type(fz) == "mixed")
end


do
  local t = table.freeze({1, 2, 3, nil, 5, a = "x", b = "y", [10] = 10})
  assert(t[1] == 1 and t[3] == 3 and t[4] == nil and t.a == "x" and t[10] == 10)
  assert(rawget(t, 2) == 2 and rawget(t, "b") == "y")
  local n = #t
  assert(n == 3 or n == 5)
  assert(rawlen(t) == n)
  local count, sum = 0, 0
  for k, v in pairs(t) do
    count = count + 1
    if math.type(v) == "integer" then sum = sum + v end
  end
  assert(count == 7 and sum == 21)
  count = 0
  for i, v in ipairs(t) do count = count + 1; assert(v == i) end
  assert(count == 3)
  count = 0
  local k, v = next(t)
  while k ~= nil do count = count + 1; k, v = next(t, k) end
  assert(count == 7)
  for k in pairs(t) do
    checkvm(function () t[k] = nil end)
  end
  assert(select("#", table.unpack(t, 1, 5)) == 5)
  assert(string.format("%s", t) == tostring(t))
  assert(t == t and t ~= table.freeze({}))
end


do
  local calls = {}
  local mt = {__newindex = function (t, k, v) calls[#calls + 1] = {k, v} end,
              __index = function (t, k) return k .. "!" end,
              __len = function () return 42 end}
  local t = table.freeze(setmetatable({a = 1}, mt))
  assert(t.a == 1 and t.zz == "zz!" and #t == 42)
  t.b = 2
  t[1] = 3
  assert(#calls == 2 and calls[1][1] == "b" and calls[1][2] == 2)
  assert(calls[2][1] == 1 and calls[2][2] == 3)
  assert(rawget(t, "b") == nil and rawget(t, 1) == nil)
  checkvm(function () t.a = 5 end)
  assert(#calls == 2 and t.a == 1)

  local viarawset = table.freeze(setmetatable({}, {__newindex = rawset}))
  check(ARG, function () viarawset.x = 1 end)
  assert(rawget(viarawset, "x") == nil)

  local store = {}
  local proxy = table.freeze(setmetatable({}, {__newindex = store}))
  proxy.x = 1
  proxy[1] = 2
  assert(store.x == 1 and store[1] == 2 and rawget(proxy, "x") == nil)

  local frozenstore = table.freeze({})
  local proxy2 = setmetatable({}, {__newindex = frozenstore})
  checkvm(function () proxy2.x = 1 end)
  checkvm(function () proxy2[1] = 1 end)
  assert(next(frozenstore) == nil and next(proxy2) == nil)

  local frozenstore2 = table.freeze({10, x = 1})
  local proxy4 = setmetatable({}, {__newindex = frozenstore2})
  checkvm(function () proxy4.x = 2 end)
  checkvm(function () proxy4[1] = 2 end)
  checkvm(function () proxy4[2] = 2 end)
  assert(frozenstore2.x == 1 and frozenstore2[1] == 10 and next(proxy4) == nil)

  local chain = table.freeze({})
  local proxy3 = setmetatable({}, {__newindex = setmetatable({}, {__newindex = chain})})
  checkvm(function () proxy3.y = 1 end)

  local fmt = table.freeze({__index = function () return "mt" end, __newindex = nil})
  local o = setmetatable({}, fmt)
  assert(o.q == "mt")
  o.q = 1
  assert(o.q == 1)
  for _ = 1, 3 do assert(o.zz == "mt") end
  assert(table.isfrozen(fmt))
  local o2 = setmetatable({}, fmt)
  assert(getmetatable(o2) == fmt)
  checkvm(function () fmt.__index = nil end)

  local inner = {}
  local outer = table.freeze({inner = inner, mt = mt})
  inner.x = 1
  outer.inner.y = 2
  assert(inner.x == 1 and inner.y == 2)
  mt.__len = function () return 7 end
  assert(#t == 7)
  checkvm(function () outer.inner = {} end)

  local unfreezable = setmetatable({}, {__metatable = "locked"})
  check("cannot change a protected metatable", table.freeze, unfreezable)
  local frozenfirst = table.freeze({})
  check(ARG, setmetatable, frozenfirst, {__metatable = "locked"})
end


do
  local t = table.freeze({1, 2, 3, name = "n"})
  local function writer (i, v) t[i] = v end
  local function fieldwriter (v) t.name = v end
  local function reader (i) return t[i] end
  for i = 1, 200 do
    checkvm(writer, (i % 3) + 1, i)
    checkvm(writer, i + 3, i)
    checkvm(fieldwriter, i)
    assert(reader((i % 3) + 1) == (i % 3) + 1)
  end
  same(t, {1, 2, 3, name = "n"})

  local w = {1, 2, 3, name = "n"}
  local tabs = {w, t}
  for i = 1, 400 do
    local x = tabs[(i % 2) + 1]
    local ok = pcall(function () x[(i % 3) + 1] = -i; x.name = i end)
    assert(ok == (x == w))
  end
  assert(t[1] == 1 and t.name == "n" and w.name ~= "n")

  local acc = 0
  for _ = 1, 100 do
    for i = 1, #t do acc = acc + t[i] end
    for k, v in pairs(t) do if k == "name" then acc = acc + 1 end end
  end
  assert(acc == 700)

  local n = 0
  for i = 1, 3 do
    local ok = pcall(function () for j = i, 3 do t[j] = 0 end end)
    if not ok then n = n + 1 end
  end
  assert(n == 3 and t[3] == 3)

  local co = coroutine.wrap(function ()
    coroutine.yield(t[1])
    t[1] = 9
  end)
  assert(co() == 1)
  check(RO, co)
  assert(t[1] == 1)

  local function make ()
    local up = table.freeze({count = 0})
    return function () up.count = up.count + 1 end, function () return up.count end
  end
  local inc, get = make()
  for _ = 1, 10 do checkvm(inc) end
  assert(get() == 0)
end


do
  local w = setmetatable({}, {__mode = "k"})
  w[{}] = 1
  w.keep = true
  table.freeze(w)
  collectgarbage()
  assert(w.keep == true)
  local count = 0
  for _ in pairs(w) do count = count + 1 end
  assert(count == 1)
  local list = {}
  for i = 1, 100 do list[i] = table.freeze({i}) end
  list = nil
  collectgarbage()
end


if rawget(_G, "vec") then
  local m = mat(vec(1, 2), vec(3, 4))
  m[1] = vec(5, 6)
  assert(m[1] == vec(5, 6))
  check("table expected", table.isfrozen, m)
  check("table expected", table.freeze, vec(1, 2))
  local t = table.freeze({v = vec(1, 2, 3)})
  assert(t.v.x == 1)
  checkvm(function () t.v = vec(0, 0, 0) end)
end


if T then
  local t = table.freeze({1, 2, a = 1})
  assert(T.testC("isreadonly 2; return 1", t) == true)
  assert(T.testC("isreadonly 2; return 1", {}) == false)
  assert(T.testC("isreadonly 2; return 1", 10) == false)
  check(RO, T.testC, "pushnum 1; pushnum 2; rawset 2; return 0", t)
  check(RO, T.testC, "pushstring x; pushnum 2; rawset 2; return 0", t)
  check(RO, T.testC, "pushnum 2; rawseti 2 1; return 0", t)
  check(RO, T.testC, "pushnum 2; rawseti 2 9; return 0", t)
  check(RO, T.testC, "pushnum 2; rawsetp 2 7; return 0", t)
  check(RO, T.testC, "pushnum 2; setfield 2 a; return 0", t)
  check(RO, T.testC, "pushnum 2; setfield 2 b; return 0", t)
  check(RO, T.testC, "pushnum 2; seti 2 1; return 0", t)
  check(RO, T.testC, "pushnum 2; seti 2 5; return 0", t)
  check(RO, T.testC, "pushstring a; pushnum 2; settable 2; return 0", t)
  check(RO, T.testC, "pushnum 1; pushnum 2; settable 2; return 0", t)
  check(RO, T.testC, "newtable; setmetatable 2; return 0", t)
  check(RO, T.testC, "pushnil; setmetatable 2; return 0", t)
  same(t, {1, 2, a = 1})

  T.testC("setreadonly 2 0; return 0", t)
  assert(not table.isfrozen(t))
  t[1] = 10
  t.b = 2
  rawset(t, 3, 3)
  table.insert(t, 4)
  assert(t[1] == 10 and t.b == 2 and t[3] == 3 and t[4] == 4)
  T.testC("setreadonly 2 1; return 0", t)
  assert(table.isfrozen(t))
  checkvm(function () t[1] = 1 end)

  local proxy = table.freeze(setmetatable({}, {__newindex = function () end}))
  T.testC("pushnum 2; setfield 2 x; return 0", proxy)
  assert(rawget(proxy, "x") == nil)

  local loaded = T.testC("getfield R _LOADED; return 1")
  T.testC("pushnum 1; setglobal ROGLOBAL; return 0")
  assert(ROGLOBAL == 1)
  ROGLOBAL = nil
  local G = _G
  table.freeze(G)
  check(RO, T.testC, "pushnum 1; setglobal ROGLOBAL; return 0")
  check(RO, function () ROGLOBAL = 1 end)
  T.testC("setreadonly 2 0; return 0", G)
  ROGLOBAL = 1
  assert(ROGLOBAL == 1)
  ROGLOBAL = nil

  table.freeze(loaded)
  check("readonly _LOADED table", require, "readonly_no_such_module")
  assert(require("string") == string)
  T.testC("setreadonly 2 0; return 0", loaded)
  assert(not table.isfrozen(loaded))
  T.checkmemory()
else
  print("\n >>> testC not active: skipping read-only C API tests <<<\n")
  local loaded = package.loaded
  table.freeze(loaded)
  check("readonly _LOADED table", require, "readonly_no_such_module")
  assert(require("string") == string)
end

print("OK")

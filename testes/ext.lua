print("testing language extensions")

local function check_error (src, pattern)
  local f, msg = load(src)
  assert(f == nil, src)
  assert(string.find(msg, pattern, 1, true), msg)
end

local function line_of (src)
  local _, msg = pcall(assert(load(src, "=src")))
  return tonumber(string.match(msg, "^src:(%d+):"))
end


print("testing C comments and line numbers")

assert(line_of("local a = 1\n/* x\ny\nz */\nerror('e')") == 5)
assert(line_of("/*\n\n\n\n*/ error('e')") == 5)
assert(line_of("/* a */ local x = 1\nerror('e')") == 2)
assert(line_of("local a = 1\r\n/* x\r\ny\r\nz */\r\nerror('e')") == 5)
assert(line_of("local a = 1\n\r/* x\n\ry\n\rz */\n\rerror('e')") == 5)
assert(line_of("local a = 1\r/* x\ry\rz */\rerror('e')") == 5)
assert(line_of("/* *\n/ *\n**/\nerror('e')") == 4)
assert(line_of("/*/\n*/ error('e')") == 2)
assert(line_of("--[[\n\n]] /*\n\n*/ -- c\nerror('e')") == 6)

do
  local f = assert(load("/*\n\n*/\nreturn debug.getinfo(1, 'l').currentline"))
  assert(f() == 4)
  f = assert(load("/*\n*/ local function g ()\n/*\n\n*/ return 1\nend\nreturn g"))
  local info = debug.getinfo(f(), "S")
  assert(info.linedefined == 2 and info.lastlinedefined == 6)
  f = assert(load("/*\n\n\n*/ error('boom')", "=c"))
  local tb = select(2, xpcall(f, debug.traceback))
  assert(string.find(tb, "^c:4: boom"))
  assert(string.find(tb, "\n%s*c:4: in "))
end

check_error("local a = 1\n/* never\nclosed\n", "starting at line 2")
check_error("local a = 1\n\n/* never closed", "unfinished long comment (starting at line 3)")
assert(load("return 8 /* c */ / 2")() == 4)
assert(load("return 8 // 3")() == 2)
assert(load("return 9 /\n/* c */ 3")() == 3)


print("testing if-expressions")

do
  local a, b = 1, 2
  assert((if a < b then "lt" else "ge") == "lt")
  assert((if a > b then "gt" elseif a == b then "eq" else "lt") == "lt")
  assert((if a == 1 then "one" elseif a == 2 then "two" else "many") == "one")
  local x = if false then 1 else 2
  assert(x == 2)
  x = if nil then 1 elseif nil then 2 elseif 0 then 3 else 4
  assert(x == 3)
  x = if true then false else true
  assert(x == false)
  x = if true then nil else 1
  assert(x == nil)
  assert((if a then if b then 1 else 2 else 3) == 1)
  assert((if a then if nil then 1 else 2 else 3) == 2)
  assert(not if a then false else true)
  assert((-if a then 5 else 6) == -5)
  assert((1 + if false then 1 else 2) == 3)
  assert((2 * if false then 1 else 2 + 3) == 10)
  assert((if false then 1 else 2) + 3 == 5)
  assert((if false then 1 else 2 + 3) == 5)
  assert((if true then 1 else 2 + 3) == 1)
  assert((if a and b then a or b else 0) == 1)
  local t = {if a then "x" else "y", k = if b == 3 then 1 else 0}
  assert(t[1] == "x" and t.k == 0)
  assert(select("#", if a then a else b) == 1)
  assert(select("#", if a then a else b, if b then b else a) == 2)
  if if a then false else true then
    error("unreachable")
  end
end

do
  local a, b, c = 10, 20, 30
  local x = if false then a else 99
  assert(a == 10 and b == 20 and c == 30 and x == 99)
  x = if true then a else b
  assert(a == 10 and x == 10)
  local y = if c == 0 then a elseif c == 1 then b else c
  assert(a == 10 and b == 20 and c == 30 and y == 30)
  a = if a == 10 then b else a
  assert(a == 20 and b == 20)
end

do
  local calls = {}
  local function mark (name, v)
    calls[#calls + 1] = name
    return v
  end
  local x = if mark("c1", false) then mark("t1", 1)
            elseif mark("c2", true) then mark("t2", 2)
            elseif mark("c3", true) then mark("t3", 3)
            else mark("e", 4)
  assert(x == 2)
  assert(table.concat(calls, ",") == "c1,c2,t2")
  calls = {}
  x = if mark("c1", nil) then mark("t1", 1) else mark("e", 5)
  assert(x == 5 and table.concat(calls, ",") == "c1,e")
  calls = {}
  x = if mark("c1", 1) then mark("t1", 1) else mark("e", 5)
  assert(x == 1 and table.concat(calls, ",") == "c1,t1")
end

do
  local function three () return 1, 2, 3 end
  local function count (...) return select("#", ...) end
  assert(count(if true then three() else 0) == 1)
  assert(count(if false then 0 else three()) == 1)
  local function va (...) return if ... then ... else "none" end
  assert(count(va(1, 2, 3)) == 1 and va(7, 8) == 7 and va() == "none")
  local t = {if true then three() else 0}
  assert(#t == 1 and t[1] == 1)
  local function ret () return if true then three() else 0 end
  assert(count(ret()) == 1)
end

do
  local fs = {}
  for i = 1, 3 do
    fs[i] = if i % 2 == 1 then function () return i end else function () return -i end
  end
  assert(fs[1]() == 1 and fs[2]() == -2 and fs[3]() == 3)
  local up = 5
  local g = function (c) return if c then up else up * 2 end
  assert(g(true) == 5 and g(false) == 10)
  up = 6
  assert(g(true) == 6)
end

do
  local parts = {"local n = ... return if n == 0 then 0"}
  for i = 1, 200 do
    parts[#parts + 1] = string.format(" elseif n == %d then %d", i, i * 2)
  end
  parts[#parts + 1] = " else -1"
  local f = assert(load(table.concat(parts)))
  assert(f(0) == 0 and f(1) == 2 and f(200) == 400 and f(201) == -1)
end

do
  local s = 0
  for i = 1, 10 do
    if i % 2 == 0 then s = s + i elseif i == 5 then s = s + 100 else s = s - 1 end
  end
  assert(s == 30 + 100 - 4)
end

do
  local t, u = {v = 1, m = |self| self.v}, {v = 2, m = |self| self.v * 10}
  local c = false
  assert((if c then t else u):m() == 20)
  local x = 1
  x += if c then 10 else 20
  assert(x == 21)
  assert((if c then t else nil)?.v == nil)
  assert((if not c then t else nil)?.v == 1)
  local s = 0
  for i = if c then 5 else 1, if c then 0 else 3 do s = s + i end
  assert(s == 6)
  local v in (if c then t else u)
  assert(v == 2)
  local k <const> = 3
  assert((if k == 3 then k else 0) == 3)
  assert((if `hello` == `HELLO` then 1 else 0) == 1)
  local big = {}
  for i = 1, 199 do big[i] = "local l" .. i .. " = " .. i end
  big[#big + 1] = "return if l1 == 1 then l199 else l2, |x| x + l198"
  local a, g = assert(load(table.concat(big, "\n")))()
  assert(a == 199 and g(1) == 199)
end

check_error("return if true then 1", "'else' expected")
check_error("return if true then 1 elseif false then 2", "'else' expected")
check_error("return if true 1 else 2", "'then' expected")
check_error("return if true then 1 else 2 end", "<eof> expected")
check_error("x = if true then 1 end", "'else' expected")
check_error("return if then 1 else 2", "unexpected symbol")


print("testing lambdas")

do
  local f = |x| x * 2
  assert(f(21) == 42)
  assert((|| 42)() == 42)
  assert((| | 43)() == 43)
  assert((|a, b| a + b)(2, 3) == 5)
  assert((|a, b, c| c)(1, 2) == nil)
  assert((|...| select("#", ...))(1, nil, 3) == 3)
  assert(select("#", (|...| ...)(1, 2, 3)) == 3)
  assert(select("#", (|a, ...| ...)(1, 2, 3)) == 2)
  assert((|...t| t.n)(4, 5, 6) == 3)
  assert((|a, ...t| t[1] + a)(1, 2) == 3)
  local add = |x| |y| x + y
  assert(add(1)(2) == 3)
  local cmp = |n| if n < 0 then -n else n
  assert(cmp(-3) == 3 and cmp(4) == 4)
  local t = {5, 2, 8, 1}
  table.sort(t, |a, b| a > b)
  assert(table.concat(t, ",") == "8,5,2,1")
  assert(string.gsub("abc", ".", |c| c:upper()) == "ABC")
  local hexadump = |s| s:gsub(".", |c| string.format("%02X ", c:byte()))
  assert(hexadump("\221\255Z") == "DD FF 5A ")
  local g = |x| x | 1
  assert(g(4) == 5)
  local h = |x| x, 7
  assert(h(1) == 1)
  local tb = {|x| x + 1, |x| x + 2}
  assert(#tb == 2 and tb[2](1) == 3)
  local m = {v = 3, get = |self| self.v}
  assert(m:get() == 3)
end

do
  local function three () return 1, 2, 3 end
  assert(select("#", (|| three())()) == 3)
  assert(select("#", (|| (three()))()) == 1)
  local function probe () return debug.getinfo(1, "t").istailcall end
  assert((|| probe())() == true)
  assert((function () return probe() end)() == true)
  assert((|| (probe()))() == false)
  local loop
  loop = |n| if n == 0 then "done" else loop(n - 1)
  assert(loop(100) == "done")
  local deep
  deep = |n| n == 0 or deep(n - 1)
  assert(deep(100) == true)
  local tc
  tc = |n, k| k(n)
  assert(tc(5, |v| v * 2) == 10)
end

do
  local n = 0
  local inc = || n + 1
  n = 10
  assert(inc() == 11)
  local acc = {}
  local push = |v| table.insert(acc, v)
  push(1) push(2)
  assert(#acc == 2)
  local fs = {}
  for i = 1, 3 do fs[i] = || i * 10 end
  assert(fs[1]() == 10 and fs[3]() == 30)
  local counter = (function ()
    local c = 0
    return || (function () c = c + 1 return c end)()
  end)()
  assert(counter() == 1 and counter() == 2)
end

do
  local f = assert(load("return |x| x + 1"))
  local g = assert(load("return function(x) return x + 1 end"))
  assert(string.dump(f(), true) == string.dump(g(), true))
  assert(string.dump(f, true) == string.dump(g, true))
  f = assert(load("return |a, ...| a(...)"))
  g = assert(load("return function(a, ...) return a(...) end"))
  assert(string.dump(f(), true) == string.dump(g(), true))
  f = assert(load("return |...t| t.n"))
  g = assert(load("return function(...t) return t.n end"))
  assert(string.dump(f(), true) == string.dump(g(), true))
  local l = assert(load("return |a,\nb|\na +\nb\n\n"))()
  local info = debug.getinfo(l, "S")
  assert(info.linedefined == 1 and info.lastlinedefined == 4)
  local ok, msg = pcall(assert(load("return || 1 + error('x')", "=lam"))())
  assert(not ok and string.find(msg, "^lam:1: x"))
end

do
  local a, b = 6, 1
  assert(a | b == 7)
  assert((a | b) == 7)
  assert(a|b == 7)
  assert(1 | 2 | 4 == 7)
  local v = 1
  v |= 6
  assert(v == 7)
  local f = assert(load("local a, b = ... return a | |x| x"))
  local ok, msg = pcall(f, 1)
  assert(not ok and string.find(msg, "bitwise operation on a function value"))
  f = assert(load("local a, b, c = ... return a || b | c"))
  ok, msg = pcall(f, 1, 2, 3)
  assert(not ok and string.find(msg, "bitwise operation on a function value"))
end

check_error("return a || b", "'|' expected")
check_error("return || ", "unexpected symbol")
check_error("return |1| 1", "<name> or '...' expected")
check_error("return |a b| a", "'|' expected")
check_error("return |a,| a", "<name> or '...' expected")
check_error("return |..., a| a", "'|' expected")
check_error("return |a| local x = 1", "unexpected symbol")
check_error("return |a| return a", "unexpected symbol")
check_error("return || ...", "cannot use '...' outside a vararg function")
check_error("x = |a|", "unexpected symbol")
check_error("local a = 1 a | 2", "syntax error")


print("testing extended number literals")

do
  assert(1_000_000 == 1000000 and math.type(1_000_000) == "integer")
  assert(0xFF_FF == 65535 and math.type(0xFF_FF) == "integer")
  assert(0b1010 == 10 and math.type(0b1010) == "integer")
  assert(0B11 == 3)
  assert(0b0 == 0 and 0b1 == 1)
  assert(0b0_01111111_000_0000 == 0x3F80)
  assert(0b1111111111111111111111111111111111111111111111111111111111111111 == -1)
  assert(0b10000000000000000000000000000000000000000000000000000000000000000 == 0)
  assert(0b0111111111111111111111111111111111111111111111111111111111111111 == math.maxinteger)
  assert(1f == 1.0 and math.type(1f) == "float")
  assert(1F == 1.0 and math.type(1F) == "float")
  assert(1.5f == 1.5 and .5f == 0.5 and 1.f == 1.0)
  assert(1e2f == 100.0 and 1E-1F == 0.1 and 2.5e+1f == 25.0)
  assert(1_0f == 10.0 and math.type(1_0f) == "float")
  assert(9007199254740993f == 9007199254740992.0)
  assert(0x1f == 31 and math.type(0x1f) == "integer")
  assert(0x1F == 31 and 0xff == 255)
  assert(0x1.8p1 == 3.0)
  assert(0x1_0p1_0 == 0x10p10)
  assert(1__0 == 10 and 1_ == 1 and 1._5 == 1.5)
  assert(1e_5 == 1e5 and 0x_FF == 255 and 0_b1 == 1)
  assert(3.1_4_1_5 == 3.1415)
  assert(1_000.000_1 == 1000.0001)
  assert(math.type(1_000.0) == "float")
  assert(-0b1 == -1 and -1f == -1.0)
  assert(0b11 + 1 == 4 and 2f * 2 == 4.0)
  local t = {}
  t[1_0] = "ten"
  assert(t[10] == "ten")
  assert(load("return 1_000")() == 1000)
end

do
  local function malformed (s)
    check_error("return " .. s, "malformed number")
  end
  malformed("0b2")
  malformed("0b")
  malformed("0B")
  malformed("0b1.0")
  malformed("0b1f")
  malformed("0b1e1")
  malformed("0b12")
  malformed("0x1p4f")
  malformed("1ff")
  malformed("1fx")
  malformed("1f5")
  malformed("1e5ff")
  malformed("1e5f5")
  malformed("1ef")
  malformed("1_x")
  malformed("0x")
  malformed("0x_")
  malformed("1g")
  malformed("1fe")
  check_error("return 0b2", "malformed number near '0b2'")
  check_error("return 1ff", "malformed number near '1ff'")
  check_error("return 1_000_z", "malformed number near '1000z'")
end

do
  assert(tonumber("0b101") == nil)
  assert(tonumber("1f") == nil)
  assert(tonumber("1.5f") == nil)
  assert(tonumber("1_000") == nil)
  assert(tonumber("0x1f") == 31)
  assert(tonumber("10") + 1 == 11)
  assert(not pcall(function () return "1f" + 1 end))
  assert(math.tointeger("0b1") == nil)
  assert(string.format("%d", 0b100) == "4")
  assert(1 .. 2 == "12")
end


print("testing table.fill and table.compact")

do
  local t = table.fill(5, "x")
  assert(#t == 5 and t[1] == "x" and t[5] == "x" and t[6] == nil)
  assert(table.type(t) == "array")
  local v = {}
  t = table.fill(3, v)
  assert(t[1] == v and t[3] == v and #t == 3)
  t = table.fill(4)
  assert(next(t) == nil and #t == 0)
  t = table.fill(0, 1)
  assert(next(t) == nil)
  t = table.fill(3, false)
  assert(t[1] == false and t[3] == false and #t == 3)
  t[4] = 1
  assert(#t == 4)
  assert(not pcall(table.fill, -1, 1))
  assert(not pcall(table.fill, "x"))

  for _, mode in ipairs({"incremental", "generational"}) do
    local old = collectgarbage(mode)
    t = table.fill(100, {})
    collectgarbage()
    t[50] = {}
    collectgarbage()
    assert(type(t[1]) == "table" and t[1] == t[100] and t[50] ~= t[1])
    collectgarbage(old)
  end
end

do
  local t = {}
  for i = 1, 100000 do t[i] = i end
  for i = 11, 100000 do t[i] = nil end
  t.k = "v"
  collectgarbage()
  collectgarbage()
  local before = collectgarbage("count")
  assert(table.compact(t) == t)
  collectgarbage()
  collectgarbage()
  assert(before - collectgarbage("count") > 500)
  assert(#t == 10 and t[10] == 10 and t[11] == nil and t.k == "v")
  local n = 0
  for _ in pairs(t) do n = n + 1 end
  assert(n == 11)
  t[11] = 11
  assert(#t == 11 and t[11] == 11)
  assert(table.compact(t) == t and #t == 11 and t.k == "v")

  local h = {1, 2, 3, 4, 5, 6, 7, 8}
  h[3] = nil
  table.compact(h)
  for i = 1, 8 do assert(h[i] == (i ~= 3 and i or nil)) end
  n = 0
  for _ in pairs(h) do n = n + 1 end
  assert(n == 7)

  local e = {1, 2, 3}
  e[1], e[2], e[3] = nil, nil, nil
  table.compact(e)
  assert(next(e) == nil and table.type(e) == "empty")

  local hash = {}
  for i = 1, 1000 do hash["k" .. i] = i end
  for i = 2, 1000 do hash["k" .. i] = nil end
  table.compact(hash)
  assert(hash.k1 == 1 and next(hash, "k1") == nil and table.type(hash) == "hash")
  hash.k2 = 2
  assert(hash.k2 == 2)

  local empty = {}
  assert(table.compact(empty) == empty and next(empty) == nil)
  assert(not pcall(table.compact, 1))
end

print("OK")

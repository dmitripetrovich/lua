print("testing language extensions")

local debug = require "debug"

local function check_error (src, pattern)
  local f, msg = load(src)
  assert(f == nil, src)
  assert(string.find(msg, pattern, 1, true), msg)
end

local function line_of (src)
  local _, msg = pcall(assert(load(src, "=src")))
  return tonumber(string.match(msg, "^src:(%d+):"))
end

local function compiles (src)
  local f, err = load(src)
  return f ~= nil, err
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
  local f = assert(load("/*\n\n*/\nreturn require'debug'.getinfo(1, 'l').currentline"))
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
  local function samecode (a, b)  -- (no dumps under LUA_NO_DUMP)
    local ok, da = pcall(string.dump, a, true)
    return not ok or da == string.dump(b, true)
  end
  assert(samecode(f(), g()))
  assert(samecode(f, g))
  f = assert(load("return |a, ...| a(...)"))
  g = assert(load("return function(a, ...) return a(...) end"))
  assert(samecode(f(), g()))
  f = assert(load("return |...t| t.n"))
  g = assert(load("return function(...t) return t.n end"))
  assert(samecode(f(), g()))
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


print("testing 'in' destructuring")

do
  local t = {k = 10, c = 20, d = 30}
  local k, c, d in (t)
  assert(k == 10 and c == 20 and d == 30)
  local x in (t)
  assert(x == nil)
  local k2, c2 in t  -- a plain local is indexed in place
  assert(k2 == nil and c2 == nil)
  local t2 = {k3 = 1, t2 = 2}
  local k3, t2 in t2  -- the new 't2' is not visible yet: the outer one is read
  assert(k3 == 1 and t2 == 2)
  local up = {k4 = 5}
  local function fromup () local k4 in up return k4 end  -- an upvalue
  assert(fromup() == 5)
  k, c, d = 0, 0, 0
  k, c, d in (t)
  assert(k == 10 and c == 20 and d == 30)
  -- with over 256 constants the names no longer fit in an instruction,
  -- so the earlier targets load them into registers
  local pad = {}
  for i = 1, 300 do pad[i] = string.format("%q", "p" .. i) end
  local src = "local t = ... local pad = {" .. table.concat(pad, ", ") .. "} "
  for _, body in ipairs{"local k, c, d in (t) return k, c, d", "local k, c, d = 0, 0, 0 k, c, d in (t) return k, c, d", "k, c, d in (t) return k, c, d"} do
    local a, b, e = assert(load(src .. body, "=in", "t", {}))(t)
    assert(a == 10 and b == 20 and e == 30, body)
  end
end


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


print("testing string.strtrim")

do
  assert(string.strtrim("  x y \t\n") == "x y" and string.strtrim("") == "" and string.strtrim(" \t ") == "")
  assert(string.strtrim("xxaxx", "x") == "a" and string.strtrim("abc", "") == "abc")
  -- NUL bytes are content, not delimiters
  assert(string.strtrim("\0a\0") == "\0a\0" and string.strtrim(" \0 ") == "\0")
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


print("testing integer formatting")

do
  local function same (i)
    local s = string.format("%d", i)
    assert(tostring(i) == s, s)
    assert(i .. "" == s and "" .. i == s, s)
  end
  for _, i in ipairs({0, 1, -1, 9, 10, -10, 99, 100, -100, 101, 999, 1000, 12345, -98765, 4294967295, 4294967296, math.maxinteger, math.mininteger, math.maxinteger - 1, math.mininteger + 1}) do
    same(i)
  end
  local p = 1
  for _ = 0, 18 do
    same(p); same(p - 1); same(p + 1); same(-p); same(-p + 1); same(-p - 1)
    p = p * 10
  end
  math.randomseed(42)
  for _ = 1, 200000 do
    same(math.random(math.mininteger, math.maxinteger))
    same(math.random(-100000, 100000))
  end
  assert(tostring(1.0) == "1.0" and tostring(-0.5) == "-0.5")
  assert(string.format("%s|%s", 7, -7) == "7|-7")
end

print("testing string interpolation")

if load('return $"x"') then
  local function check_error (src, pattern)
    local f, msg = load(src)
    assert(f == nil, src)
    assert(string.find(msg, pattern, 1, true), msg)
  end
  local name, n = "bob", 3
  assert(load('return $"hello"')() == "hello" and load("return $'it'")() == "it")
  assert(load('local name, n = ... return $"hi {name}, {n} items"')(name, n) == "hi bob, 3 items")
  assert(load('local n = ... return $"{n}"')(n) == "3")
  assert(load('return $"{nil} {true} {1.5} {2^53 | 0}"')() == "nil true 1.5 9007199254740992")
  assert(load('return $"{1}{2}{3}{4}"')() == "1234")
  assert(load('local t = ... return $"<{t}>"')(setmetatable({}, { __tostring = function () return "T" end })) == "<T>")
  assert(load('local n = ... return $"a{ $"b{n}c" }d"')(n) == "ab3cd")
  assert(load('return $"{ #{1, 2, {3}} } { ({x = 1}).x }"')() == "3 1")
  assert(load('return $"{ "}" }{ \'{\' }"')() == "}{")
  assert(load('return $"\\{x\\} {1}"')() == "{x} 1")
  assert(load('local name = ... return $"{ name:upper() }!"')(name) == "BOB!")
  assert(load('return $"{ (function () return 1, 2 end)() }"')() == "1")
  assert(load('return $"\\t{1}\\n\\u{41}"')() == "\t1\nA")
  assert(load('return $"{vec2(1, 2)}"')() == tostring(vec2(1, 2)))
  assert(load('local s = "" for i = 1, 3 do s = s .. $"[{i}]" end return s')() == "[1][2][3]")
  local tostring = nil
  assert(load('local tostring = nil return $"{1}"')() == "1")
  do  -- no dependency on string.format; same conversions as tostring
    local format = string.format
    string.format = nil
    assert(load('return $"{1}-{nil}"')() == "1-nil")
    string.format = format
  end
  assert(load('return $"{1.0}|{-0.0}|{2^63}|{math.mininteger}"')() == "1.0|-0.0|" .. _G.tostring(2^63) .. "|-9223372036854775808")
  local named = setmetatable({}, { __name = "Thing" })
  assert(load('local t = ... return $"{t}"')(named) == _G.tostring(named) and _G.tostring(named):find("^Thing: "))
  local plain = {}
  assert(load('local t = ... return $"{t}"')(plain) == _G.tostring(plain))
  assert(load('return $"{print}"')() == _G.tostring(print))
  assert(load('local m = ... return $"{m}"')(mat2()) == _G.tostring(mat2()))
  assert(load('local t = ... return $"{t}"')(setmetatable({}, { __tostring = function () return 42 end })) == "42")
  local bad = setmetatable({}, { __tostring = function () return {} end })
  local ok, err = pcall(load('local t = ... return $"{t}"'), bad)
  assert(not ok and err:find("'__tostring' must return a string"))
  local calls = 0
  local counted = setmetatable({}, { __tostring = function (...) calls = calls + select("#", ...) return "c" end })
  assert(load('local t = ... return $"{t}{t}"')(counted) == "cc" and calls == 2)
  check_error('return $"{}"', "empty expression")
  check_error('return $"{1 2}"', "'}' expected")
  check_error('return $"abc', "unfinished string")
  check_error('return $"a\nb"', "unfinished string")
  check_error('return $"{1"', "unfinished string")
  check_error('return $', "unexpected symbol")
  check_error('return $"' .. string.rep('{$"', 40) .. '"', "nested too deeply")
else
  print("\n >>> string interpolation not compiled: skipping its tests <<<\n")
end

print("testing string.buffer")

if string.buffer then
  local b = string.buffer()
  assert(#b == 0 and b:tostring() == "" and tostring(b) == "")
  assert(b:put("a", 1, 2.5, true, nil) == b)
  assert(b:tostring() == "a12.5truenil" and #b == 12)
  b:put(setmetatable({}, { __tostring = function () return "T" end }), vec2(1, 2))
  assert(b:tostring() == "a12.5truenilT" .. tostring(vec2(1, 2)))
  assert(b:reset() == b and #b == 0 and tostring(b) == "")
  b:put("ab"):put(b):put(b)
  assert(tostring(b) == "abababab")
  local other = string.buffer(4):put("xyz")
  b:reset():put(other, "-", other)
  assert(tostring(b) == "xyz-xyz" and tostring(other) == "xyz")
  assert(b:reset():putf("%d+%s=%.1f", 1, "two", 3):tostring() == "1+two=3.0")
  local big = string.buffer()
  for i = 1, 100000 do big:put(i, ",") end
  local parts = {}
  for i = 1, 100000 do parts[i] = i .. "," end
  assert(big:tostring() == table.concat(parts) and #big == #table.concat(parts))
  big:reset()
  big:put(string.rep("z", 3 << 20))
  assert(#big == 3 << 20 and big:tostring() == string.rep("z", 3 << 20))
  assert(big:put("\0x\0"):tostring():sub(-3) == "\0x\0")
  assert(not pcall(string.buffer, -1))
  assert(not pcall(b.put, "notabuffer", "x"))
  assert(not pcall(b.putf, b, "%d", "x"))
  assert(not pcall(b.put, b, setmetatable({}, { __tostring = function () return {} end })))
  assert(getmetatable(b).__name == "string.buffer")
else
  print("\n >>> string.buffer not compiled: skipping its tests <<<\n")
end

print("testing startswith, endswith, math.sign and math.lerp")

if string.startswith then
  assert(("hello"):startswith("he") and ("hello"):startswith("hello") and ("hello"):startswith(""))
  assert(not ("hello"):startswith("hello!") and not ("hello"):startswith("el") and not (""):startswith("a"))
  assert(("hello"):endswith("lo") and ("hello"):endswith("hello") and ("hello"):endswith(""))
  assert(not ("hello"):endswith("!hello") and not ("hello"):endswith("ll"))
  assert(("a\0b"):startswith("a\0") and ("a\0b"):endswith("\0b") and not ("a\0b"):startswith("a\0c"))
  assert(string.startswith(123, "12") and string.endswith(123, 3))
  assert(not pcall(string.startswith, "x", {}) and not pcall(string.endswith, nil, "x"))

  assert(math.sign(5) == 1 and math.type(math.sign(5)) == "integer")
  assert(math.sign(-7) == -1 and math.sign(0) == 0 and math.type(math.sign(0)) == "integer")
  assert(math.sign(math.mininteger) == -1 and math.sign(math.maxinteger) == 1)
  assert(math.sign(2.5) == 1.0 and math.type(math.sign(2.5)) == "float")
  assert(math.sign(-0.1) == -1.0 and math.sign(0.0) == 0.0 and math.sign(-0.0) == 0.0)
  assert(math.sign(0/0) == 0 and math.sign(math.huge) == 1 and math.sign(-math.huge) == -1)
  assert(not pcall(math.sign, "x"))

  assert(math.lerp(0, 10, 0.5) == 5.0 and math.lerp(0, 10, 0) == 0 and math.lerp(0, 10, 1) == 10)
  assert(math.lerp(1e300, -1e300, 1) == -1e300 and math.lerp(0.1, 0.7, 1) == 0.7)
  assert(math.lerp(0, 10, 2) == 20 and math.lerp(0, 10, -1) == -10)
  assert(math.lerp(vec3(0, 0, 0), vec3(2, 4, 6), 0.5) == vec3(1, 2, 3))
else
  print("\n >>> startswith/endswith/sign/lerp not compiled: skipping their tests <<<\n")
end

print("testing nil-coalescing")

if load("return nil ?? 1") then
  assert(load("return nil ?? 1")() == 1)
  assert(load("return false ?? 1")() == false)
  assert(load("return 0 ?? 1")() == 0 and load("return '' ?? 1")() == "")
  local calls = 0
  local function side () calls = calls + 1 return "x" end
  local pick = load("local a, side = ... return a ?? side()")
  assert(pick(5, side) == 5 and calls == 0)
  assert(pick(nil, side) == "x" and calls == 1)
  assert(load("local a, b, c = ... return a ?? b ?? c")(nil, nil, 3) == 3)
  assert(load("local a, b, c = ... return a ?? b ?? c")(nil, false, 3) == false)
  assert(load("local a, b = ... return a ?? b or 'z'")(nil, false) == "z")
  assert(load("local a = ... return a ?? 1 + 2")(nil) == 3)
  assert(load("local a = ... return a == nil ?? 'no'")(nil) == true)
  assert(load("local t = ... return t?.a?.b ?? 'none'")(nil) == "none")
  assert(load("local t = ... return t?.a?.b ?? 'none'")({ a = { b = false } }) == false)
  assert(load("local a = ... return { a ?? 1, a ?? 2, (a ?? 3) * 2 }")(nil)[3] == 6)
  assert(select("#", load("return nil ?? (function () return 1, 2 end)()")()) == 1)
  local guarded = setmetatable({}, { __eq = function () error("__eq called") end })
  assert(load("local o = ... return o ?? 1")(guarded) == guarded)
  assert(load("local t = ... return t.missing ?? t.present")({ present = 7 }) == 7)

  local x, y, a, one, g, f = load([[
    local t, x, y = {}, nil, 5
    x ??= 10
    y ??= 10
    t.a ??= 1
    t.a ??= 2
    t[1] ??= "one"
    COALESCE_GLOBAL ??= "g"
    COALESCE_GLOBAL ??= "h"
    local f = false
    f ??= true
    return x, y, t.a, t[1], COALESCE_GLOBAL, f
  ]])()
  assert(x == 10 and y == 5 and a == 1 and one == "one" and g == "g" and f == false)
  COALESCE_GLOBAL = nil
  local frozen = table.freeze({ v = 1 })
  assert(load("local t = ... t.v ??= 2 return t.v")(frozen) == 1)
  assert(not pcall(load("local t = ... t.w ??= 2"), frozen))
  calls = 0
  load("local t, side = ... t.k ??= side() t.k ??= side()")({}, side)
  assert(calls == 1)

  local src = { "local a = ... local t = {" }
  for i = 1, 300 do src[#src + 1] = ("'k%d',"):format(i) end
  src[#src + 1] = "} local b b ??= 'set' return a ?? 'deep', b"
  local deep, set = load(table.concat(src))(nil)
  assert(deep == "deep" and set == "set")

  assert(not load("return ?? 1") and not load("x ??= ") and not load("f() ??= 1") and not load("return 1 ??= 2"))
  assert(load("local t = ... return t?.v")({ v = 4 }) == 4 and load("local t = ... return t?.v")(nil) == nil)
else
  print("\n >>> nil-coalescing not compiled: skipping its tests <<<\n")
end

print("testing default parameter values")

if load("local function f (a = 1) return a end") then
  local f = load("return function (a, b = 10, c = 's', d = -1, e = true, g = nil, h = 2.5, i = 2^10) return a, b, c, d, e, g, h, i end")()
  local a, b, c, d, e, g, h, i = f(1)
  assert(a == 1 and b == 10 and c == "s" and d == -1 and e == true and g == nil and h == 2.5 and i == 1024.0)
  a, b, c = f(1, false, "x")
  assert(b == false and c == "x")
  assert(debug.getinfo(f).nparams == 8)

  local derived = load("return function (a, b = a * 2, c = { a, b }) return a, b, c end")()
  a, b, c = derived(3)
  assert(a == 3 and b == 6 and c[1] == 3 and c[2] == 6)
  a, b, c = derived(3, 1)
  assert(b == 1 and c[2] == 1)

  local fresh = load("return function (t = {}) return t end")()
  assert(fresh() ~= fresh() and next(fresh()) == nil)
  local given = {}
  assert(fresh(given) == given)

  local calls = 0
  local lazy = load("local make = ... return function (x = make()) return x end")(function () calls = calls + 1 return "d" end)
  assert(lazy(1) == 1 and lazy() == "d" and lazy(false) == false and lazy() == "d" and calls == 2)

  DEFAULT_NAME = "anon"
  local named = load("return function (name = DEFAULT_NAME) return name end")()
  assert(named() == "anon")
  DEFAULT_NAME = "other"
  assert(named() == "other")
  DEFAULT_NAME = nil

  local obj = load("local o = { base = 5 } function o:get (x = self.base, y = x + 1) return x, y end return o")()
  a, b = obj:get()
  assert(a == 5 and b == 6)
  a, b = obj:get(1)
  assert(a == 1 and b == 2)

  local va = load("return function (a = 1, b = a + 1, ...) return a, b, select('#', ...), ... end")()
  local r = table.pack(va())
  assert(r[1] == 1 and r[2] == 2 and r[3] == 0)
  r = table.pack(va(nil, nil, "x", "y"))
  assert(r[1] == 1 and r[2] == 2 and r[3] == 2 and r[4] == "x" and r[5] == "y")

  local fact = load("local function fact (n, acc = 1) if n <= 1 then return acc end return fact(n - 1, acc * n) end return fact")()
  assert(fact(5) == 120)
  local nested = load("return function (f = function (x = 2) return x * 3 end) return f() end")()
  assert(nested() == 6)

  assert(not load("return function (a = ) end"))
  assert(not load("return function (... = 1) end"))
  assert(not load("return function (a = 1 2) end"))
  if load("return |x| x") then assert(not load("return |a = 1| a")) end
  local many = { "return function (" }
  for k = 1, 33 do many[#many + 1] = ("p%d = %d,"):format(k, k) end
  many[#many + 1] = "z) end"
  local fn, msg = load(table.concat(many))
  assert(fn == nil and msg:find("too many parameters with default values", 1, true))
else
  print("\n >>> default parameter values not compiled: skipping their tests <<<\n")
end

print("testing switch")

if load("switch 1 end") then
  local classify = load([[
    local x = ...
    switch x
    case 1, 2, 3 then return "small"
    case "a" then return "letter"
    case nil then return "nothing"
    else return "other"
    end
  ]])
  assert(classify(1) == "small" and classify(3) == "small" and classify(1.0) == "small")
  assert(classify("a") == "letter" and classify(nil) == "nothing" and classify(9) == "other" and classify("1") == "other")

  local calls = 0
  local once = load("local f = ... local r switch f() case 1 then r = 'one' case 2 then r = 'two' else r = 'none' end return r")
  assert(once(function () calls = calls + 1 return 2 end) == "two" and calls == 1)
  local count = load("local x = ... local t = {} switch x case 1 then t[#t + 1] = 'a' case 2 then t[#t + 1] = 'b' end return #t")
  assert(count(1) == 1 and count(2) == 1 and count(5) == 0)

  local mt = { __eq = function (a, b) return a.id == b.id end }
  local A, B = setmetatable({ id = 1 }, mt), setmetatable({ id = 1 }, mt)
  assert(load("local a, b = ... switch a case b then return true end return false")(A, B) == true)
  assert(load("local x, k = ... switch x case k + 1 then return 'next' end")(5, 4) == "next")

  local loop = load([[
    local out = {}
    for i = 1, 6 do
      switch i % 3
      case 0 then
        local label = "fizz"
        switch i case 6 then label = label .. "!" end
        out[#out + 1] = label
      case 1 then
        if i > 3 then break end
        out[#out + 1] = i
      else
        out[#out + 1] = "-"
      end
    end
    return table.concat(out, ",")
  ]])
  assert(loop() == "1,-,fizz")
  if load("for i = 1, 1 do continue end") then
    assert(load("local s = 0 for i = 1, 5 do switch i case 2, 4 then continue end s = s + i end return s")() == 9)
  end
  local fns = load("local fs = {} for i = 1, 3 do switch i case 2 then local v = i * 10 fs[#fs + 1] = function () return v end end end return fs")()
  assert(#fns == 1 and fns[1]() == 20)

  assert(load("switch 1 end return true")())
  assert(load("local x = 0 switch x else x = 1 end return x")() == 1)
  assert(not load("switch 1 case then end"))
  assert(not load("switch 1 case 1 end"))
  assert(not load("switch 1 else case 1 then end"))
  assert(not load("switch 1 case 1 then"))
  assert(not load("case 1 then end"))
  assert(not load("local case = 1") and not load("local switch = 1"))
else
  print("\n >>> switch not compiled: skipping its tests <<<\n")
end


print("testing a named vararg parameter as an assignment target")

do
  local f = load("local function f (...t) t.x, t.y = 1, 2 local x, y; x, y in t return x, y end return f")
  if f then
    local x, y = f()()
    assert(x == 1 and y == 2)
  end
  f = load("local function f (...t) t[1] += 10 t.n -= 1 return t[1], t.n end return f")
  if f then
    local a, n = f()(5)
    assert(a == 15 and n == 0)
  end
  f = load("local function f (...t) t[1] ??= 7 t[2] ??= 7 return t[1], t[2] end return f")
  if f then
    local a, b = f()(1)
    assert(a == 1 and b == 7)
  end
end


print("testing compound assignment")

do
  local x = 10
  x += 5
  assert(x == 15)
  x -= 3
  assert(x == 12)
  x -= -1
  assert(x == 13)
  x *= 2 + 1
  assert(x == 39)
  x /= 6
  assert(x == 6.5 and math.type(x) == "float")
  x=1
  x+=1
  assert(x == 2)
  local i = 3
  i <<= 4
  assert(i == 48)
  i >>= 2
  assert(i == 12)
  i &= 10
  assert(i == 8)
  i |= 3
  assert(i == 11)
  -- '^=' is exponentiation, like '^'
  i ^= 2
  assert(i == 121.0 and math.type(i) == "float")
  i = 11
  local n = -1
  n >>= 63
  assert(n == 1)
  local s = 2.0
  s <<= 1
  assert(s == 4 and math.type(s) == "integer")

  local up = 1
  local function bump () up += 2 end
  bump()
  bump()
  assert(up == 5)

  COMPOUND_GLOBAL = 5
  COMPOUND_GLOBAL *= 3
  COMPOUND_GLOBAL -= 1
  assert(COMPOUND_GLOBAL == 14)
  COMPOUND_GLOBAL = nil

  local t = { a = 1, [2] = 10 }
  t.a += 1
  t[2] -= 4
  assert(t.a == 2 and t[2] == 6)
  local k = 2
  t[k] *= 3
  assert(t[2] == 18)
  local calls = 0
  local function key () calls = calls + 1 return "a" end
  t[key()] += 10
  assert(t.a == 12 and calls == 1)
  local gets = 0
  local function tab () gets = gets + 1 return t end
  tab().a |= 1
  tab()[2] /= 2
  assert(t.a == 13 and t[2] == 9 and gets == 2)
  local m = { { 1, 2 }, { 3, 4 } }
  local rows = 0
  local function row () rows = rows + 1 return 2 end
  m[row()][1] += 100
  assert(m[2][1] == 103 and m[1][1] == 1 and rows == 1)

  local log = {}
  local proxy = setmetatable({}, { __index = function (_, f) log[#log + 1] = "get " .. f return 1 end, __newindex = function (_, f, v) log[#log + 1] = "set " .. f .. "=" .. v end })
  proxy.v += 41
  assert(#log == 2 and log[1] == "get v" and log[2] == "set v=42")
  local obj = setmetatable({}, { __add = function (a, b) return type(a) .. "+" .. type(b) end })
  local o = obj
  o += 1
  assert(o == "table+number")

  if table.freeze then
    local frozen = table.freeze({ v = 1 })
    assert(not pcall(assert(load("local t = ... t.v += 1")), frozen))
    assert(not pcall(assert(load("local t = ... t.v |= 2")), frozen))
    assert(frozen.v == 1)
  end
  local ok = pcall(assert(load("local t = ... t.v += 1")), nil)
  assert(not ok)
end

check_error("local k <const> = 1 k += 1", "attempt to assign to const variable 'k'")
check_error("local k <const> = 1 k <<= 1", "attempt to assign to const variable 'k'")
check_error("f() += 1", "syntax error")
check_error("(x) += 1", "syntax error")
check_error("x += ", "unexpected symbol")
check_error("x, y += 1", "'=' expected")


print("testing '::' field selection and dumping")

if load("local t = {} return t::x") then  -- GRIT_POWER_NAMESPACE_SEL
  local ns = { sub = { f = function (x) return x * 2 end, v = 7 } }
  local a, b, c = load("local ns = ... return ns::sub::v, ns::sub.v, ns.sub::f(4)")(ns)
  assert(a == 7 and b == 7 and c == 8)
  assert(load("local ns = ... function ns::sub::g () return 1 end return ns.sub.g()")(ns) == 1)
  -- labels still work at the start of a statement
  assert(load("local n = 0 ::top:: n = n + 1 if n < 3 then goto top end return n")() == 3)
  assert(load("for i = 1, 3 do if i == 2 then goto continue end ::continue:: end return true")())
  assert(load("local n = 0 for i = 1, 3 do n = n + i\n ::continue:: end return n")() == 6)
  assert(load("local t = {f = function () return {g = 5} end} return t::f()::g")() == 5)
end

do  -- string.dump either works or fails cleanly (LUA_NO_DUMP), never an empty chunk
  local ok, res = pcall(string.dump, function () return 1 end)
  if ok then
    assert(#res > 0)
    -- the format byte (after "\27Lua" and the version) must match this build
    local f = load(res, "chunk", "b")
    if f then
      assert(f() == 1)
      local other = res:sub(1, 5) .. string.char(res:byte(6) ~ 0x0F) .. res:sub(7)
      local g, msg = load(other, "chunk", "b")
      assert(g == nil and msg:find("format mismatch"))
    end
  else
    assert(res:find("cannot dump functions"))
  end
end


print("testing continue, imports, fast calls, table.find and string.split")

do
  local t = {}
  for i = 1, 10 do
    if i % 2 == 0 then continue end
    t[#t + 1] = i
  end
  assert(table.concat(t, ",") == "1,3,5,7,9")

  local sum = 0
  local i = 0
  while i < 10 do
    i = i + 1
    if i % 3 == 0 then continue end
    sum = sum + i
  end
  assert(sum == 37)

  local r = 0
  local seen = 0
  repeat
    r = r + 1
    if r < 5 then continue end
    seen = seen + 1
  until r >= 7
  assert(r == 7 and seen == 3)

  local keys = 0
  for k, v in pairs({a = 1, b = 2, c = 3}) do
    if v == 2 then continue end
    keys = keys + 1
  end
  assert(keys == 2)

  local odd = 0
  for _, v in ipairs({1, 2, 3, 4, 5}) do
    if v % 2 == 0 then continue end
    odd = odd + v
  end
  assert(odd == 9)

  local pairsum = 0
  for a = 1, 3 do
    for b = 1, 3 do
      if b == a then continue end
      pairsum = pairsum + a * 10 + b
    end
    if a == 2 then continue end
    pairsum = pairsum + 1000
  end
  assert(pairsum == 2132)

  local fs = {}
  for k = 1, 5 do
    local x = k * 10
    fs[#fs + 1] = function () return x end
    if k % 2 == 0 then continue end
    x = x + 1
  end
  assert(fs[1]() == 11 and fs[2]() == 20 and fs[3]() == 31 and fs[4]() == 40 and fs[5]() == 51)

  local wfs = {}
  local w = 0
  while w < 4 do
    w = w + 1
    local y = w
    wfs[w] = function () return y end
    if w == 2 then continue end
    y = -y
  end
  assert(wfs[1]() == -1 and wfs[2]() == 2 and wfs[3]() == -3 and wfs[4]() == -4)

  local rfs = {}
  local rr = 0
  repeat
    rr = rr + 1
    local z = rr
    rfs[rr] = function () return z end
    if rr == 2 then continue end
    z = z * 100
  until rr == 3
  assert(rfs[1]() == 100 and rfs[2]() == 2 and rfs[3]() == 300)

  local closed = 0
  local mt = {__close = function () closed = closed + 1 end}
  for k = 1, 4 do
    local c <close> = setmetatable({}, mt)
    if k % 2 == 0 then continue end
  end
  assert(closed == 4)

  local ok = 0
  repeat
    local done = ok >= 3
    if not done then
      ok = ok + 1
      continue
    end
  until done
  assert(ok == 3)
end

do
  local continue = 1
  continue = continue + 1
  assert(continue == 2)
  local t = {continue = 3}
  t.continue = t.continue + 1
  assert(t.continue == 4)
  local function continue2 () return "called" end
  local calls = {continue = continue2}
  assert(calls.continue() == "called")
  assert(compiles("local continue = function () end continue()"))
  assert(compiles("continue = 1"))
  assert(compiles("local t = {} t.continue = 1"))
  assert(compiles("local continue, x = 1, 2"))
  local okc, err = compiles("continue")
  assert(not okc and string.find(err, "continue outside loop"))
  okc, err = compiles("for i = 1, 2 do local f = function () continue end end")
  assert(not okc and string.find(err, "continue outside loop"))
  okc, err = compiles("repeat if x then continue end local y = 1 until y")
  assert(not okc and string.find(err, "local 'y' used in the repeat..until condition"))
  assert(compiles("repeat if x then continue end local y = 1 until x"))
  assert(compiles("repeat local y = 1 if y then continue end until y"))
  okc, err = compiles("repeat local a = 1 if a then continue end local b = 2 local c = 3 until c")
  assert(not okc and string.find(err, "local 'c'"))
  local n = 0
  for i = 1, 5 do
    if i == 3 then goto continue end
    n = n + i
    ::continue::
  end
  assert(n == 12)
  local m = 0
  for i = 1, 3 do
    for j = 1, 3 do
      if j == 2 then goto next end
      m = m + 1
      ::next::
    end
    if i == 2 then continue end
    m = m + 10
  end
  assert(m == 26)
end

do
  local function floor (x) return math.floor(x) end
  local function upper (s) return string.upper(s) end
  assert(floor(1.5) == 1 and upper("a") == "A")
  local old = math.floor
  math.floor = function () return 42 end
  assert(floor(1.5) == 42)
  math.floor = old
  assert(floor(1.5) == 1)
  local oldmath = math
  math = {floor = function () return 7 end}
  assert(floor(1.5) == 7)
  math = oldmath
  assert(floor(1.5) == 1)
  rawset(math, "floor", function () return 8 end)
  assert(floor(1.5) == 8)
  rawset(math, "floor", old)
  assert(floor(1.5) == 1)
  local proxy = setmetatable({}, {__newindex = math})
  math.floor = nil
  proxy.floor = function () return 9 end
  assert(floor(1.5) == 9)
  math.floor = old
  assert(floor(1.5) == 1)
  _ENV.math = {floor = function () return 10 end}
  assert(floor(1.5) == 10)
  _ENV.math = oldmath
  assert(floor(1.5) == 1 and upper("b") == "B")

  lib = {x = {y = 1, f = function () return "f1" end}}
  local function y () return lib.x.y end
  local function f () return lib.x.f() end
  assert(y() == 1 and f() == "f1")
  lib.x.y = 2
  assert(y() == 2)
  lib.x = {y = 3, f = function () return "f2" end}
  assert(y() == 3 and f() == "f2")
  function lib.x.f () return "f3" end
  assert(f() == "f3")
  lib.x.y, lib.x.z = 4, 5
  assert(y() == 4 and lib.x.z == 5)
  lib.x.y += 1
  assert(y() == 5)
  table.clear(lib.x)
  assert(y() == nil)
  lib.x.y = 6
  assert(y() == 6)
  table.clear(lib)
  assert(not pcall(y))
  lib = nil

  local function make (env)
    local _ENV = env
    return function () return a.b end
  end
  local g1 = make({a = {b = 1}})
  local g2 = make({a = {b = 2}})
  assert(g1() == 1 and g2() == 2 and g1() == 1 and g2() == 2)

  do
    local _ENV = {assert = assert, math = {floor = function () return "local env" end}}
    assert(math.floor(1) == "local env")
  end
  assert(math.floor(1.5) == 1)

  local inherit = setmetatable({}, {__index = _G})
  local h = load("return math.floor(2.5), string.rep('a', 2)", "h", "t", inherit)
  local a1, a2 = h()
  assert(a1 == 2 and a2 == "aa")
  inherit.math = {floor = function () return "own" end}
  assert(h() == "own")
  inherit.math = nil
  assert(h() == 2)

  local idx = setmetatable({}, {__index = function (_, k) if k == "lib2" then return {v = {w = "dynamic"}} end end})
  local dyn = load("return lib2.v.w", "d", "t", idx)
  assert(dyn() == "dynamic")

  local weakenv = setmetatable({lib3 = {v = "weak"}}, {__mode = "k"})
  local wk = load("return lib3.v", "w", "t", weakenv)
  assert(wk() == "weak")
  weakenv.lib3 = {v = "changed"}
  assert(wk() == "changed")

  local meta = {}
  local menv = setmetatable({lib4 = {v = 1}}, meta)
  local mk = load("return lib4.v", "m", "t", menv)
  assert(mk() == 1)
  meta.__mode = "v"
  menv.lib4.v = 2
  assert(mk() == 2)
  setmetatable(menv, nil)
  menv.lib4 = {v = 3}
  assert(mk() == 3)

  local envf = load("return lib5.v", "e", "t", {lib5 = {v = "first"}})
  assert(envf() == "first")
  debug.setupvalue(envf, 1, {lib5 = {v = "second"}})
  assert(envf() == "second")

  local frozen = table.freeze({v = {w = 1}})
  local fz = load("return lib6.v.w", "z", "t", {lib6 = frozen})
  assert(fz() == 1)

  local co = coroutine.wrap(function ()
    coroutine.yield(math.floor(3.5))
    coroutine.yield(math.floor(3.5))
  end)
  assert(co() == 3)
  math.floor = function () return "co" end
  assert(co() == "co")
  math.floor = old

  counter = 0
  local function bump () for _ = 1, 100 do counter = counter + 1 assert(math.floor(counter + 0.5) == counter) end end
  bump()
  assert(counter == 100)
  counter = nil

  -- writes of keys no import reads keep the cache; writes of import keys
  -- still reach it, however the key string was made
  local oldlen = string.len
  local function len2 () return string.len("ab") end
  assert(len2() == 2)
  for i = 1, 10 do unrelated = i; string.unrelated = i end
  assert(len2() == 2)
  string[("le") .. ("n")] = function () return "built key" end
  assert(len2() == "built key")
  for i = 1, 10 do unrelated = i; string.unrelated = i end
  rawset(string, "len", function () return "raw" end)
  assert(len2() == "raw")
  string.len, string.unrelated, unrelated = oldlen, nil, nil
  assert(len2() == 2)

  -- other keys (and integer keys) written into an imported table take the
  -- fast path and keep the cache; a __newindex chain into it updates
  -- existing keys in place and still invalidates on import keys
  M9 = {f = function () return 1 end, g = 1}
  local callf = load("return M9.f()")
  assert(callf() == 1)
  M9[1] = "one"
  M9.other = 2
  M9.g = 3
  assert(callf() == 1 and M9[1] == "one" and M9.other == 2 and M9.g == 3)
  local proxy = setmetatable({}, {__newindex = M9})
  proxy.g = 4
  proxy.h = 5
  local nkeys = 0
  for _ in pairs(M9) do nkeys = nkeys + 1 end
  assert(M9.g == 4 and M9.h == 5 and nkeys == 5)  -- f, g, [1], other, h
  proxy.f = function () return 2 end
  assert(callf() == 2)
  -- a plain value is read fresh every time (it is not cached)
  M9.n = 1
  local getn = load("return M9.n")
  for i = 1, 3 do
    assert(getn() == i)
    M9.n = i + 1
  end
  M9 = nil

  local meta2 = {}
  local menv2 = setmetatable({lib9 = {v = 1}}, meta2)
  local mk2 = load("return lib9.v", "m2", "t", menv2)
  assert(mk2() == 1)
  rawset(meta2, "__mode", "v")
  collectgarbage()
  assert(menv2.lib9 == nil and not pcall(mk2))

  local okm, msg = pcall(load("return nosuch.field.deep"))
  assert(not okm and string.find(msg, "global 'nosuch'"))
  local okn, msgn = pcall(load("local t = {} t.a = nil return lib7.a.b"))
  assert(not okn and string.find(msgn, "global 'lib7'"))
  lib8 = {}
  local okf, msgf = pcall(load("return lib8.a.b"))
  assert(not okf and string.find(msgf, "field 'a'"))
  lib8 = nil
end

do
  local function norm (v)
    if type(v) == "string" then
      return (v:gsub("^%[string .-%]:%d+: ", ""):gsub("'[%w%.%[%]\"]+'", "'f'"))
    end
    return v
  end
  local function same (f, g, ...)
    local a = table.pack(pcall(f, ...))
    local b = table.pack(pcall(g, ...))
    if a.n ~= b.n then return false end
    for i = 1, a.n do
      local x, y = norm(a[i]), norm(b[i])
      if math.type(x) ~= math.type(y) or type(x) ~= type(y) then return false end
      if x ~= y and not (x ~= x and y ~= y) then return false end
    end
    return true
  end
  local alias = {}
  local names = {"math.abs", "math.floor", "math.ceil", "math.sqrt", "math.min", "math.max", "math.sin", "math.cos", "math.clamp", "type", "rawequal", "rawget", "string.len", "string.byte", "vec2", "vec3", "vec4", "length", "distance", "dot"}
  for _, name in ipairs(names) do
    alias[name] = load("return " .. name)()
  end
  local function pair (name, params)
    local fast = load("return function (" .. params .. ") local t = table.pack(" .. name .. "(" .. params .. ")) return table.unpack(t, 1, t.n) end")()
    local slow = load("local alias = ... return function (" .. params .. ") local t = table.pack(alias['" .. name .. "'](" .. params .. ")) return table.unpack(t, 1, t.n) end")(alias)
    return fast, slow
  end
  local values = {0, -0.0, 1, -1, 1.5, -1.5, 2^53, -2^63, math.mininteger, math.maxinteger, math.huge, -math.huge, 0/0, "3.5", "x", true, false, {}, vec3(1, 2, 3), vec3(4, 6, 3), vec2(3, 4)}
  for _, name in ipairs(names) do
    local f0, s0 = pair(name, "")
    assert(same(f0, s0), name)
    local f1, s1 = pair(name, "x")
    local f2, s2 = pair(name, "x, y")
    local f3, s3 = pair(name, "x, y, z")
    for _, v in ipairs(values) do
      assert(same(f1, s1, v), name)
      for _, w in ipairs(values) do
        assert(same(f2, s2, v, w), name)
      end
      assert(same(f3, s3, v, 2, 3), name)
      assert(same(f3, s3, 1, v, 3), name)
    end
    assert(same(f1, s1, nil), name)
    assert(same(f2, s2, nil, nil), name)
  end
  local fr, sr = pair("rawget", "x, y")
  local idx = setmetatable({}, {__index = function () return 1 end})
  assert(same(fr, sr, {a = 1}, "a") and same(fr, sr, idx, "a") and same(fr, sr, 1, 2))
  local fb, sb = pair("string.byte", "x, y")
  for _, i in ipairs({-5, -3, -1, 0, 1, 2, 3, 4, 9}) do
    assert(same(fb, sb, "abc", i) and same(fb, sb, "", i))
  end
  local fm, sm = pair("math.max", "x, y, z, w")
  assert(same(fm, sm, 1, 2.5, 2, 0/0) and same(fm, sm, 0/0, 1, 2, 3))
  assert(select("#", math.floor(1.5)) == 1)
  assert(select("#", string.byte("abc", 10)) == 0)
  local a, b = math.floor(1.5)
  assert(a == 1 and b == nil)
  local packed = {math.floor(2.5), string.byte("abc", 10)}
  assert(#packed == 1 and packed[1] == 2)
  local saved = math.floor
  math.floor = function () return "replaced" end
  local t = table.pack(math.floor(1))
  assert(t[1] == "replaced")
  math.floor = saved
  local calls = 0
  debug.sethook(function () calls = calls + 1 end, "c")
  for _ = 1, 10 do local r = math.floor(1.5) end
  debug.sethook()
  assert(calls >= 10)
end

do
  assert(table.find({10, 20, 30}, 20) == 2)
  assert(table.find({10, 20, 30}, 40) == nil)
  assert(table.find({10, 20, 10}, 10, 2) == 3)
  assert(table.find({1, 2, nil, 4}, 4) == nil)
  assert(table.find({}, nil) == nil)
  local eq = setmetatable({}, {__eq = function () return true end})
  assert(table.find({setmetatable({}, getmetatable(eq))}, eq) == nil)
  assert(not pcall(table.find, {1}, 1, 0))
  assert(not pcall(table.find, nil, 1))
  assert(not pcall(table.find, {1}))
  local function parts (s, sep)
    local t = sep == nil and string.split(s) or string.split(s, sep)
    return table.concat(t, "|"), #t
  end
  local s, n = parts("a,b,,c")
  assert(s == "a|b||c" and n == 4)
  s, n = parts("a--b--c", "--")
  assert(s == "a|b|c" and n == 3)
  s, n = parts("abc", "")
  assert(s == "a|b|c" and n == 3)
  s, n = parts("", ",")
  assert(s == "" and n == 1)
  s, n = parts("", "")
  assert(n == 0)
  s, n = parts("a,", ",")
  assert(s == "a|" and n == 2)
  s, n = parts("abc", "x")
  assert(s == "abc" and n == 1)
  s, n = parts("a\0b\0c", "\0")
  assert(n == 3)
  s, n = parts("aaa", "aa")
  assert(s == "|a" and n == 2)
  assert(not pcall(string.split))
  assert(("x;y"):split(";")[2] == "y")
end

if table.clone then
  -- cloning into a metatable refreshes its cache of absent metamethods
  local mt = {}
  local obj = setmetatable({}, mt)
  assert(obj.missing == nil)
  table.clone({ __index = function () return "hit" end }, mt)
  assert(obj.missing == "hit")
end

if utf8.strcmputf8i then
  local cmp = utf8.strcmputf8i
  assert(cmp("ab", "ac") == -1 and cmp("ac", "ab") == 1)
  assert(cmp("ABC", "abc") == 0 and cmp("abc", "ABD") == -1)
  assert(cmp("a", "ab") == -1 and cmp("ab", "a") == 1 and cmp("", "") == 0)
  assert(cmp("ÄÖÜ", "äöü") == 0 and cmp("Привет", "привет") == 0 and cmp("ΣΩ", "σω") == 0)
  assert(cmp("é", "f") == 1 and cmp("a\0b", "a\0c") == -1)
  assert(cmp("\xff", "\xfe") == 1)
end

if string.strsplit then
  local a, b = string.strsplit(",", "a\0b,c")
  assert(a == "a\0b" and b == "c" and select("#", string.strsplit(",", "a\0b,c")) == 2)
  local x, y, z = string.strsplit(",;", "1,2;3")
  assert(x == "1" and y == "2" and z == "3")
  local first, rest = string.strsplit(",", "1,2,3", 2)
  assert(first == "1" and rest == "2,3")
  assert(string.strjoin(",") == "" and string.strjoin(",", 5) == "5" and string.strjoin("", "a", 1) == "a1")
  assert(string.strjoin("-", "a", "b") == "a-b" and string.strjoin("-", "a\0b", "c") == "a\0b-c")
  assert(not pcall(string.strjoin, ",", {}) and not pcall(string.strjoin, ",", "a", {}))
  assert(select("#", string.strsplit(",", "abc")) == 1 and string.strsplit(",", "") == "")
  local p, q, r = string.strsplit(",", "a,,b")
  assert(p == "a" and q == "" and r == "b")
  local head, tail = string.strsplit(",", "a,")
  assert(head == "a" and tail == "" and string.strsplit(",", "a,b", 1) == "a,b")
end

if string.strconcat then
  assert(string.strconcat() == "" and string.strconcat("x") == "x")
  assert(string.strconcat("a", 1, "b", 2.5) == "a1b2.5")
  local cat = setmetatable({}, { __concat = function (a, b) return "<" .. type(a) .. "," .. type(b) .. ">" end })
  assert(string.strconcat("a", cat) == "<string,table>")
  assert(not pcall(string.strconcat, "a", {}) and not pcall(string.strconcat, "a", nil))
end

if string.tostringall then
  local named = setmetatable({}, { __tostring = function () return "named" end })
  local a, b, c, d, e = string.tostringall(1, nil, true, "s", named)
  assert(a == "1" and b == "nil" and c == "true" and d == "s" and e == "named")
  assert(select("#", string.tostringall()) == 0 and select("#", string.tostringall(nil, nil)) == 2)
  assert(string.find(string.tostringall({}), "^table: "))
end

if string.blob then
  local b = string.blob(4)
  assert(type(b) == "string" and #b == 4 and string.isblob(b))
  assert(not string.isblob("abcd") and not string.isblob(4) and not string.isblob())
  assert(b:byte(1) == 0 and b:byte(4) == 0)
  -- a blob never equals a regular string, even with the same contents
  assert(b ~= "\0\0\0\0" and b == string.blob(4))
  assert(not pcall(string.blob, 0) and not pcall(string.blob, -1) and not pcall(string.blob, {}))
  local s = "abc"
  local c = string.blob(s)
  assert(string.isblob(c) and #c == 3 and c:sub(1) == "abc" and not string.isblob(s))
  -- packing in place when the result fits
  local r = string.blob_pack(b, "<i2", 0x0102)
  assert(r == b and string.isblob(r) and string.unpack("<i2", b) == 0x0102)
  r = string.blob_pack(b, 3, "<i2", 7)
  assert(r == b and string.unpack("<i2", b, 3) == 7 and string.unpack("<i2", b) == 0x0102)
  -- a result that does not fit makes a new, longer blob; the original keeps its size
  local g = string.blob_pack(b, 4, "<i4", 9)
  assert(string.isblob(g) and #g == 7 and #b == 4)
  assert(string.unpack("<i2", g) == 0x0102 and string.unpack("<i4", g, 4) == 9)
  local v, nextpos = string.blob_unpack(g, 4, "<i4")
  assert(v == 9 and nextpos == 8)
  v, nextpos = string.blob_unpack(g, "<i2")
  assert(v == 0x0102 and nextpos == 3)
  assert(string.blob_unpack("\5\0", "<i2") == 5)
  assert(not pcall(string.blob_pack, "abcd", "i2", 1))
  assert(not pcall(string.blob_pack, b, 6, "i2", 1) and not pcall(string.blob_unpack, g, 9, "i2"))
end

if utf8.len16 then
  assert(utf8.len16("") == 0 and utf8.len16("abc") == 3)
  assert(utf8.len16("é€") == 2 and utf8.len16("😀") == 2 and utf8.len16("a😀b") == 4)
  assert(utf8.len16("a😀b", 2, 5) == 2 and utf8.len16("abc", 2) == 2 and utf8.len16("abc", 3, 2) == 0)
  assert(utf8.len16("\xe2\x82") == -1 and utf8.len16("a\xf0\x9f") == -1)
  assert(not pcall(utf8.len16, "abc", 5) and not pcall(utf8.len16, "abc", 1, 4))
end

if scrub then
  local co = coroutine.create(print)
  local n = select("#", scrub(1, "a", true, nil, {}, print, co, io.stdout))
  assert(n == 8)
  local a, b, c, d, e, f, g, h = scrub(1, "a", true, nil, {}, print, co, io.stdout)
  assert(a == 1 and b == "a" and c == true and d == nil)
  assert(e == nil and f == nil and g == nil and h == nil)
  assert(select("#", scrub()) == 0)
  if vec3 then
    assert(scrub(vec3(1, 2, 3)) == vec3(1, 2, 3) and scrub(mat3()) == nil)
  end
end

if each then
  local t = { 10, 20, x = 30 }
  local f, s, c = each(t)
  assert(f == next and s == t and c == nil)
  local sum = 0
  for _, v in each(t) do sum = sum + v end
  assert(sum == 60)
  local counted = setmetatable({}, { __iter = function (self) return function (_, i) if i < 3 then return i + 1 end end, self, 0 end })
  local seen = {}
  for i in each(counted) do seen[#seen + 1] = i end
  assert(#seen == 3 and seen[1] == 1 and seen[3] == 3)
  local paired = setmetatable({}, { __pairs = function (self) return function (_, k) if not k then return 1, "a" end end, self, nil end })
  local keys = 0
  for k, v in each(paired) do
    keys = keys + 1
    assert(k == 1 and v == "a")
  end
  assert(keys == 1)
  assert(not pcall(each))
end

if os.microtime then
  local m1 = os.microtime()
  local n = os.nanotime and os.nanotime()
  local m2 = os.microtime()
  assert(math.type(m1) == "integer" and m1 <= m2)
  if n then
    -- one clock: a nanosecond sample taken between two microsecond samples lies between them
    assert(math.type(n) == "integer" and m1 <= n // 1000 and n // 1000 <= m2)
    assert(os.nanotime() >= n)
  end
  assert(os.deltatime(10, 3) == 7 and os.deltatime(3, 10) == 7 and os.deltatime(5, 5) == 0)
  assert(os.deltatime(m2, m1) == m2 - m1)
  assert(not pcall(os.deltatime, 1) and not pcall(os.deltatime, 1.5, 1))
  if os.rdtsc then
    assert(math.type(os.rdtsc()) == "integer" and math.type(os.rdtscp()) == "integer")
  end
end

if math.cbrt then
  local inf = math.huge
  local function isnan (x) return x ~= x end
  local function near (a, b) return math.abs(a - b) <= 1e-12 * math.max(1, math.abs(b)) end
  assert(math.eps == 2.0^-52 and math.feps == 2.0^-23)
  assert(near(math.cbrt(27), 3) and near(math.cbrt(-8), -2) and math.cbrt(0) == 0)
  assert(math.trunc(-2.7) == -2 and math.trunc(2.7) == 2 and math.type(math.trunc(2)) == "float")
  assert(math.round(2.5) == 3 and math.round(-2.5) == -3 and math.round(2.4) == 2)
  assert(math.nearbyint(2.5) == 2 and math.nearbyint(3.5) == 4 and math.nearbyint(-2.5) == -2)
  assert(math.copysign(3, -0.0) == -3 and math.copysign(-3, 1) == 3 and 1 / math.copysign(0, -1) == -inf)
  assert(math.fdim(5, 3) == 2 and math.fdim(3, 5) == 0)
  assert(math.hypot(3, 4) == 5 and math.hypot(inf, 0 / 0) == inf)
  assert(math.exp2(10) == 1024 and math.exp2(-1) == 0.5 and math.logb(1024) == 10 and math.logb(0) == -inf)
  assert(math.scalbn(1, 10) == 1024 and math.scalbn(3, -1) == 1.5 and not pcall(math.scalbn, 1, 0.5))
  assert(math.expm1(0) == 0 and near(math.expm1(1), math.exp(1) - 1))
  assert(math.log1p(0) == 0 and near(math.log1p(math.exp(1) - 1), 1) and math.log1p(-1) == -inf)
  assert(math.erf(0) == 0 and math.erfc(0) == 1 and near(math.erf(1), 0.8427007929497149))
  assert(math.erf(inf) == 1 and math.erfc(inf) == 0)
  assert(near(math.gamma(5), 24) and near(math.gamma(0.5), math.sqrt(math.pi)) and math.gamma(0) == inf)
  assert(math.lgamma(1) == 0 and math.lgamma(2) == 0 and near(math.lgamma(10), math.log(362880)))
  assert(math.remainder(5, 3) == -1 and math.remainder(7, 2) == -1 and math.remainder(8, 3) == -1)
  assert(math.nextafter(1, 2) == 1 + math.eps and math.nextafter(1, 0) == 1 - math.eps / 2 and math.nextafter(1, 1) == 1)
  assert(math.asinh(0) == 0 and math.acosh(1) == 0 and math.atanh(0) == 0 and math.atanh(1) == inf)
  assert(near(math.asinh(1), math.log(1 + math.sqrt(2))) and isnan(math.acosh(0.5)))
  assert(math.isnan(0 / 0) and not math.isnan(1) and not math.isnan(inf))
  assert(math.isinf(inf) and math.isinf(-inf) and not math.isinf(1) and not math.isinf(0 / 0))
  assert(math.isfinite(1) and math.isfinite(0) and not math.isfinite(inf) and not math.isfinite(0 / 0))
  assert(math.isnormal(1) and not math.isnormal(0) and not math.isnormal(inf) and not math.isnormal(math.scalbn(1, -1074)))
  assert(isnan(math.cbrt(0 / 0)) and isnan(math.round(0 / 0)) and math.trunc(-inf) == -inf)
  assert(not pcall(math.cbrt, "x") and not pcall(math.hypot, 1))
end


do  -- a local alias of a library function takes the fast path too
  local sqrt, floor, len, max = math.sqrt, math.floor, string.len, math.max
  assert(sqrt(16) == 4.0 and floor(2.5) == 2 and len("abc") == 3 and max(1, 3, 2) == 3)
  assert(math.type(floor(2.5)) == "integer" and select("#", max(1)) == 1)
  local min = string.len  -- the name of another fast function: an ordinary call
  assert(min("abcd") == 4)
  local abs = function (x) return "mine", x end  -- not the library function
  local r, x = abs(-1)
  assert(r == "mine" and x == -1)
end


do  -- defer(fn) / func2close(fn): a to-be-closed object that calls 'fn'
  local df = rawget(_G, "defer") or rawget(_G, "func2close")
  if df then
    local log = {}
    do
      local a <close> = df(function (obj, err) log[#log + 1] = type(obj) .. tostring(err) end)
      local b <close> = df(function () log[#log + 1] = "b" end)
    end
    assert(log[1] == "b" and log[2] == "tablenil")
    local ok, msg = pcall(function ()
      local c <close> = df(function (_, err) log[#log + 1] = err end)
      error("boom", 0)
    end)
    assert(not ok and msg == "boom" and log[3] == "boom")
    local co = coroutine.wrap(function ()  -- the function may yield
      local d <close> = df(function () coroutine.yield("in close") end)
    end)
    assert(co() == "in close")
    co()
    assert(not pcall(df, 1))
    assert(getmetatable(df(print)) == getmetatable(df(print)))
  end
end

print("OK")

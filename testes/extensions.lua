print("testing continue, imports, fast calls, table.find and string.split")

local function compiles (src)
  local f, err = load(src)
  return f ~= nil, err
end

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

print("OK")

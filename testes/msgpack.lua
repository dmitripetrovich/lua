print("testing msgpack")

local json = require "json"
local msgpack = require "msgpack"

local function fails (f, ...)
  return not pcall(f, ...)
end

local function deepeq (a, b)
  if type(a) ~= type(b) then return false end
  if type(a) ~= "table" then
    if a ~= a and b ~= b then return true end
    return a == b and math.type(a) == math.type(b)
  end
  for k, v in pairs(a) do if not deepeq(v, b[k]) then return false end end
  for k in pairs(b) do if a[k] == nil then return false end end
  return true
end


print("testing msgpack round trips")

for _, v in ipairs({ 0, 1, -1, 127, 128, -32, -33, 2^40 | 0, -(2^40 | 0), math.maxinteger, math.mininteger, 1.5, -0.25, 1e300, "", "abc", string.rep("z", 70000), true, false }) do
  assert(deepeq(msgpack.unpack(msgpack.pack(v)), v))
end

do
  local nested = { a = { 1, 2, { b = "c" } }, [10] = false, list = { 1.5, "x", {} } }
  assert(deepeq(msgpack.unpack(msgpack.pack(nested)), nested))
  local big = {}
  for i = 1, 1000 do big["k" .. i] = i end
  assert(deepeq(msgpack.unpack(msgpack.pack(big)), big))
  local a, b, c = msgpack.unpack(msgpack.pack(1, "two", { 3 }))
  assert(a == 1 and b == "two" and deepeq(c, { 3 }))
end

assert(msgpack.getoption("small_lua") == false and msgpack.getoption("full64bits") == true)


print("testing msgpack packers")

do
  local p = msgpack.new()
  p:array({ 1, 2 }, { 3 })
  local a1, a2 = msgpack.unpack(tostring(p))
  assert(deepeq(a1, { 1, 2 }) and deepeq(a2, { 3 }))
  p = msgpack.new()
  p:integer(2^40 | 0)
  assert(msgpack.unpack(tostring(p)) == 2^40 | 0)
  assert(type(msgpack.packers.int) == "function")
  assert(fails(p.uint8, p, "x"))
  assert(fails(p.uint8, p, 1.5))
  assert(fails(p.binary, p, {}))
  assert(fails(p.float, p, "y"))
  p = msgpack.new()
  p:string(12)
  assert(msgpack.unpack(tostring(p)) == "12")
  assert(fails(function () return p .. {} end))
  p = msgpack.new()
  p:integer(7)
  local before = #p
  assert(fails(function () return p({ 1, function () end }) end))
  assert(fails(p.array, p, { 2, function () end }))
  assert(#p == before and msgpack.unpack(tostring(p)) == 7)
  p:integer(8)
  local x, y = msgpack.unpack(tostring(p))
  assert(x == 7 and y == 8)
end


print("testing msgpack options")

msgpack.setoption("integer", true)
assert(msgpack.unpack(msgpack.pack(1.5)) == 1.5)
assert(math.type(msgpack.unpack(msgpack.pack(3.0))) == "integer")
msgpack.setoption("double", true)
assert(math.type(msgpack.unpack(msgpack.pack(3.0))) == "float")

msgpack.setoption("with_hole", true)
do
  local clock = os.clock()
  local bomb = msgpack.unpack(msgpack.pack({ n = 1e12 | 0 }))
  assert(os.clock() - clock < 1 and bomb.n == 1e12 | 0)
  assert(#msgpack.pack({ n = 3 }) == 4)
  assert(deepeq(msgpack.unpack(msgpack.pack({ 1, 2, n = 2 })), { 1, 2 }))
end
msgpack.setoption("without_hole", true)

assert(fails(msgpack.setoption, "full64bits", true))
assert(fails(msgpack.setoption, "with_hole", 1))
assert(fails(msgpack.setoption, "bogus", true))


print("testing msgpack.with")

do
  local mp = msgpack.with({ with_hole = true, sentinel = true })
  assert(mp.pack({ 1, nil, 3 }) == "\x93\x01\xc0\x03")
  assert(msgpack.pack({ 1, nil, 3 }) ~= mp.pack({ 1, nil, 3 }))
  assert(mp.unpack("\x91\xc0")[1] == msgpack.null and msgpack.unpack("\x91\xc0")[1] == nil)
  msgpack.setoption("always_as_map", true)
  assert(mp.pack({ 1, 2 }) == "\x92\x01\x02")
  msgpack.setoption("without_hole", true)
  assert(mp.pack_args(1, nil) == "\x92\x01\xc0")
  assert(select(2, mp.next(mp.pack(5))) == 5)
  local bound = mp.new()
  bound({ 1, nil, 3 })
  assert(tostring(bound) == "\x93\x01\xc0\x03")
  assert(mp.null == msgpack.null)
  assert(msgpack.with({}).pack({ 1, nil, 3 }) == "\x82\x01\x01\x03\x03")
  assert(fails(msgpack.with, { bogus = true }))
  assert(fails(msgpack.with, { with_hole = 1 }))
  assert(fails(msgpack.with, { true }))
  assert(fails(msgpack.with, { full64bits = true }))
end


print("testing msgpack decoding")

do
  local ok, dec = pcall(msgpack.unpack, "\x82\xcb\x7f\xf8\x00\x00\x00\x00\x00\x00\x01\xa1a\x02")
  assert(ok and dec.a == 2)
  assert(pcall(msgpack.unpack, string.rep("\x91", 32) .. "\x90"))
  assert(fails(msgpack.unpack, "\x92\x01"))
  assert(select("#", msgpack.next(msgpack.pack(1, 2), 1, 2^40 | 0)) >= 2)
  assert(fails(msgpack.next, msgpack.pack(1), 1, -1))
end


print("testing msgpack extensions")

do
  local Point = {}
  Point.__index = Point
  msgpack.extend({
    __ext = 5,
    __pack = function (v) return string.pack("<ii", v.x, v.y) end,
    __unpack = function (s)
      local x, y = string.unpack("<ii", s)
      return setmetatable({ x = x, y = y }, Point)
    end,
  })
  Point.__ext = 5
  local pt = msgpack.unpack(msgpack.pack(setmetatable({ x = 3, y = -4 }, Point)))
  assert(getmetatable(pt) == Point and pt.x == 3 and pt.y == -4)
  assert(fails(msgpack.settype, "table", 300))
  assert(fails(msgpack.extend_clear, 1, 20))
  msgpack.extend_clear(5)
  assert(deepeq(msgpack.unpack(msgpack.pack(vec3(1, 2, 3))), vec3(1, 2, 3)))
end


print("testing msgpack.null")

do
  assert(msgpack.null == json.null and msgpack.null == msgpack.sentinel)
  assert(msgpack.pack(json.null) == msgpack.pack(nil))
  json.setoption("null", false)
  local withnull = json.decode("[1,null,2]")
  assert(msgpack.pack(withnull) == "\x93\x01\xc0\x02")
  json.setoption("null", true)
  msgpack.setoption("sentinel", true)
  local back = msgpack.unpack("\x93\x01\xc0\x02")
  assert(back[2] == json.null and json.encode(back) == "[1,null,2]")
  msgpack.setoption("sentinel", false)
end


print("testing msgpack scanning, integers and array detection")

do
  local k31 = string.rep("k", 31)
  local t = msgpack.unpack("\x82\xa1a\x01\xbf" .. k31 .. "\xa0")
  assert(t.a == 1 and t[k31] == "")
  for _, k in ipairs({ "", "a", k31, k31 .. "k" }) do
    assert(deepeq(msgpack.unpack(msgpack.pack({ [k] = k })), { [k] = k }))
  end
  assert(fails(msgpack.unpack, "\x81\xa3ab"))
  assert(fails(msgpack.unpack, "\x81\xa1a"))
  assert(fails(msgpack.unpack, "\xa5abc"))
  assert(fails(msgpack.unpack, "\x92\xa1a"))
  assert(fails(msgpack.unpack, "\xc1"))
  assert(fails(msgpack.unpack, "\xdd\xff\xff\xff\xff"))
  assert(fails(msgpack.unpack, "\xdf\xff\xff\xff\xff\xa1a\x01"))
  assert(fails(msgpack.unpack, "\x91\xd9\x05ab"))

  for _, v in ipairs({ 0, -1, -32, -33, 127, 128, 255, 256, 65535, 65536, -(1 << 31), (1 << 32) - 1, 1 << 32, math.maxinteger, math.mininteger }) do
    local back = msgpack.unpack(msgpack.pack(v))
    assert(back == v and math.type(back) == "integer")
  end
  assert(msgpack.unpack("\xcf\xff\xff\xff\xff\xff\xff\xff\xff") == 2.0 ^ 64)
  assert(msgpack.unpack("\xd3\x80\x00\x00\x00\x00\x00\x00\x00") == math.mininteger)

  assert(msgpack.pack({}) == "\x90")
  assert(msgpack.pack({ 1, 2, 3 }) == "\x93\x01\x02\x03")
  assert(msgpack.pack({ [2] = 1 }):byte() == 0x81)
  assert(msgpack.pack({ 1, x = 1 }):byte() == 0x82)
  assert(msgpack.pack({ [0] = 1 }):byte() == 0x81)
  assert(msgpack.pack({ [1.5] = 1 }):byte() == 0x81)
  assert(msgpack.pack({ 1, 2, n = 2 }) == "\x92\x01\x02")
  assert(msgpack.pack({ 1, n = 3 }):byte() == 0x82)
  assert(msgpack.pack({ 1, n = 100 }):byte() == 0x82)
  assert(msgpack.pack({ 1, n = 0 }):byte() == 0x82)
  assert(msgpack.pack({ 1, n = "2" }):byte() == 0x82)
  msgpack.setoption("with_hole", true)
  assert(msgpack.pack({ 1, nil, 3 }) == "\x93\x01\xc0\x03")
  assert(msgpack.pack({ [20] = 1 }):byte() == 0x81)
  assert(msgpack.pack({ [16] = 1 }):byte() == 0xdc)
  msgpack.setoption("without_hole", true)
end


print("testing msgpack depth limits, empty input and packer buffers")

do
  local function chain (n)
    local t = {}
    local cur = t
    for _ = 2, n do
      cur[1] = {}
      cur = cur[1]
    end
    return t
  end
  local function deep (n)
    return string.rep("\x91", n) .. "\xc0"
  end
  assert(msgpack.getoption("max_depth") == 32 and json.getoption("max_depth") == 32)
  assert(pcall(msgpack.pack, chain(32)) and pcall(json.encode, chain(32)))
  local ok, err = pcall(msgpack.pack, chain(33))
  assert(not ok and err:find("maximum table nesting depth exceeded", 1, true))
  ok, err = pcall(json.encode, chain(33))
  assert(not ok and err:find("maximum table nesting depth exceeded", 1, true))
  local cyc = {}
  cyc[1] = cyc
  assert(fails(msgpack.pack, cyc))
  msgpack.setoption("max_depth", 50)
  assert(msgpack.getoption("max_depth") == 50)
  assert(pcall(msgpack.pack, chain(50)) and fails(msgpack.pack, chain(51)))
  msgpack.setoption("max_depth", 32)
  local mp = msgpack.with({ max_depth = 3 })
  assert(pcall(mp.pack, chain(3)) and fails(mp.pack, chain(4)))
  assert(fails(mp.new(), chain(4)))
  assert(fails(msgpack.setoption, "max_depth", 0))
  assert(fails(msgpack.setoption, "max_depth", 201))
  assert(fails(msgpack.setoption, "max_depth", true))
  assert(fails(msgpack.with, { max_depth = 1.5 }))

  assert(msgpack.getoption("decode_max_depth") == 1000 and json.getoption("decode_max_depth") == 1000)
  assert(pcall(msgpack.unpack, deep(1000)))
  ok, err = pcall(msgpack.unpack, deep(1001))
  assert(not ok and err:find("nests too deeply", 1, true))
  assert(pcall(msgpack.unpack, "\x81\xc0" .. deep(40)))
  msgpack.setoption("decode_max_depth", 32)
  assert(pcall(msgpack.unpack, deep(32)) and fails(msgpack.unpack, deep(33)))
  msgpack.setoption("decode_max_depth", 1000)
  assert(fails(msgpack.setoption, "decode_max_depth", 0))
  assert(fails(msgpack.setoption, "decode_max_depth", 1001))
  assert(fails(msgpack.with({ decode_max_depth = 2 }).unpack, deep(3)))

  ok, err = pcall(msgpack.unpack, "")
  assert(not ok and err:find("incomplete", 1, true))
  local pos, val = msgpack.next("")
  assert(pos == -1 and val == nil)
  local jv, joff, jmsg = json.decode("")
  assert(jv == nil and joff == 0 and type(jmsg) == "string")
  pos, val = msgpack.next(msgpack.pack(1), 0)
  assert(pos == 0 and val == nil)

  local p = msgpack.new()
  for i = 1, 1000 do
    p:integer(i)
  end
  local all = { msgpack.unpack(tostring(p)) }
  assert(#all == 1000 and all[1000] == 1000)
  p:string(string.rep("z", 100000))
  assert(#p > 100000)
  assert(fails(p, { 1, print }))
  p:integer(1001)
  all = { msgpack.unpack(tostring(p)) }
  assert(#all == 1002 and all[1001] == string.rep("z", 100000) and all[1002] == 1001)
  do
    local q <close> = msgpack.new()
    q:string(string.rep("q", 5000))
  end
  assert(fails(p.array, p, "not a table"))
  assert(fails(p.table, p, 5))
  assert(fails(p._table, p, 5))
end

print("OK")

print("testing json")

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

local function nest (n)
  return string.rep("[", n) .. string.rep("]", n)
end


print("testing json round trips")

do
  local doc = { name = "x\n\"y\"\1", list = { 1, 2.5, -3, true, false }, nested = { deeper = { "z" } }, huge = string.rep("q", 1 << 20) }
  assert(deepeq(json.decode(json.encode(doc)), doc))
  assert(json.encode(2^40 | 0) == "1099511627776")
  assert(json.encode(math.mininteger) == tostring(math.mininteger))
  assert(json.decode("1099511627776") == 2^40 | 0)
  assert(json.encode("a\0b") == '"a\\u0000b"')
  assert(json.decode('"\\u00e9\\ud83d\\ude00"') == "\u{e9}\u{1f600}")
end


print("testing json keys")

assert(json.encode({ [1] = "a", [3] = "b", x = 1 }, { sort_keys = true }) == '{"1":"a","3":"b","x":1}')
assert(json.encode({ [1.5] = 7, x = 1 }, { sort_keys = true }) == '{"1.5":7,"x":1}')
assert(json.encode({ b = 1, a = 2, c = 3 }, { keyorder = { "c", "a" } }) == '{"c":3,"a":2,"b":1}')
assert(json.encode({ ["a\0b"] = 1, ["a\0a"] = 2 }, { sort_keys = true }) == '{"a\\u0000a":2,"a\\u0000b":1}')

do
  local victim = {}
  local function clear ()
    for k in pairs(victim) do victim[k] = nil end
    collectgarbage()
    collectgarbage()
    return "1"
  end
  for i = 1, 20 do victim[string.rep("k", 60) .. i] = setmetatable({}, { __tojson = clear }) end
  assert(pcall(json.encode, victim, { sort_keys = true }))
end


print("testing json numbers")

do
  local function replace () return "x" end
  assert(json.encode({ 1, 0/0 }, { nan = false, exception = replace }) == '[1,"x"]')
  assert(json.encode({ a = 0/0 }, { nan = false, exception = replace }) == '{"a":"x"}')
  assert(fails(json.encode, { 0/0 }, { nan = false }))
  assert(json.encode(0/0) == "NaN")
  assert(fails(json.encode, vec3(0/0, 1, 2), { nan = false }))
  assert(json.encode(vec2(1, 2), { vectorarray = true }) == "[1.0,2.0]")
  assert(json.encode(-1e300, { lua_round_float = true }) ~= nil)
  assert(json.decode("0e400") == 0)
  assert(json.decode("1e400") == nil)
end


print("testing json.decode")

do
  local v, pos = json.decode("xx[1]", 3)
  assert(deepeq(v, { 1 }) and pos == 6)
  local nv, epos = json.decode("xx[1,", 3)
  assert(nv == nil and epos >= 2)
  assert(json.decode('{"a":1}\0junk') == nil)
  assert(json.decode("") == nil)
end


print("testing json depth")

do
  local cyc = {}
  cyc[1] = cyc
  assert(fails(json.encode, cyc))
  json.setoption("max_depth", 200)
  assert(fails(json.encode, cyc))
  local deep = {}
  local cur = deep
  for _ = 1, 150 do
    cur[1] = {}
    cur = cur[1]
  end
  assert(pcall(json.encode, deep))
  json.setoption("max_depth", 32)

  assert(json.getoption("decode_max_depth") == 1000)
  assert(json.decode(nest(1000)) ~= nil)
  local dv, doff, dmsg = json.decode(nest(1001))
  assert(dv == nil and type(doff) == "number" and dmsg:find("Nesting too deep", 1, true))
  assert(select(3, json.decode(nest(100000))):find("Nesting too deep", 1, true))
  json.setoption("decode_max_depth", 5)
  assert(json.decode(nest(5)) ~= nil and json.decode(nest(6)) == nil)
  assert(json.decode('{"a":{"b":{"c":{"d":{"e":1}}}}}') ~= nil)
  assert(json.decode('{"a":{"b":{"c":{"d":{"e":{}}}}}}') == nil)
  json.setoption("decode_max_depth", 1000000)
  assert(json.decode(nest(20000)) ~= nil)
  json.setoption("decode_max_depth", 1000)
  assert(fails(json.setoption, "decode_max_depth", 0))
  assert(fails(json.setoption, "decode_max_depth", 1000001))
  assert(json.encode({ 1 }, { decode_max_depth = 3 }) == "[1]")
end


print("testing json options")

assert(fails(json.setoption, "indent_char", 10))
assert(fails(json.setoption, "max_depth", 100000))
assert(fails(json.setoption, "decimal_count", 0))
assert(json.getoption("decimal_count") == 14)
assert(json.getoption("indent_count") == 4)
json.setoption("indent_char", 1)
assert(json.getoption("indent_count") == 1)
assert(json.encode({ 1 }, { pretty = true }) == "[\n\t1\n]")
json.setoption("indent_char", 0)
assert(json.encode({ 1 }, { pretty = true, indent_char = 1 }) == "[\n\t1\n]")
assert(fails(json.encode, { 1 }, { [1] = true, pretty = false }))  -- option names are strings
assert(json.use_lpeg == nil and json.using_lpeg == nil)


print("testing json table types")

do
  local o = (json.decode("{}"))
  o[1] = 5
  assert(json.encode(o) == '{"1":5}')
  assert(json.encode((json.decode("[]"))) == "[]")
  assert(json.encode(json.array({})) == "[]" and json.encode(json.object({})) == "{}")
  assert(json.encode(json.array({ [1] = 1, [3] = 3 })) == "[1,null,3]")
  assert(json.encode(json.array({ [1e9 | 0] = 1 })) == '{"1000000000":1}')
  assert(json.isarray(json.array({})) and json.isobject(json.object({})))
  local retyped = json.array(json.decode("{}"))
  assert(json.isarray(retyped) and json.encode(retyped) == "[]")
  assert(json.isobject((json.decode("{}"))) and json.encode((json.decode("{}"))) == "{}")
  local own = setmetatable({}, {})
  assert(json.isarray(json.array(own)) and getmetatable(own).__jsontype == "array")
end


print("testing json.null")

do
  assert(json.null == msgpack.null and type(json.null) == "userdata")
  assert(json.encode(json.null) == "null" and json.encode({ json.null }) == "[null]")
  json.setoption("null", false)
  local withnull = json.decode("[1,null,2]")
  assert(withnull[2] == json.null)
  assert(json.encode(withnull) == "[1,null,2]")
  json.setoption("null", true)
  assert((json.decode("[1,null,2]"))[2] == nil)
end

do  -- decoding is correctly rounded: any 17-digit float round-trips exactly
  math.randomseed(1234)
  for _ = 1, 5000 do
    local x = math.random() * 10.0 ^ math.random(-307, 307)
    local s = string.format("%.17g", x)
    assert(json.decode(s) == x, s)
  end
  assert(json.decode("9007199254740993.0") == 9007199254740992.0)
  assert(json.decode("1.7976931348623157e308") == 1.7976931348623157e308)
  assert(json.decode("4.9406564584124654e-324") == 4.9406564584124654e-324)
end

do  -- integers are written two digits at a time
  for _, v in ipairs({ 0, 1, -1, 9, 10, -10, 99, 100, 101, -100, 1000, 12345, 1234567890123, math.maxinteger, math.mininteger }) do
    local s = json.encode(v)
    assert(s == string.format("%d", v), s)
    local back = json.decode(s)
    assert(back == v and math.type(back) == "integer", s)
  end
  local keyed = json.decode(json.encode({ [100] = "a", [-5] = "b", [0] = "c", [math.mininteger] = "d" }))
  assert(keyed["100"] == "a" and keyed["-5"] == "b" and keyed["0"] == "c" and keyed[string.format("%d", math.mininteger)] == "d")
end

do  -- array detection
  assert(json.encode({ 1, 2, 3 }) == "[1,2,3]")
  assert(json.encode({ [1] = 1, [2] = 2 }) == "[1,2]")
  assert(json.encode({ [2] = 1 }) == "[null,1]")
  assert(json.encode({ [20] = 1 }) == '{"20":1}')
  assert(json.encode({ [2] = 1, [20] = 1, [12] = 1, [13] = 1, [14] = 1, [15] = 1, [16] = 1, [17] = 1, [18] = 1, [19] = 1 }):sub(1, 1) == "[")
  assert(json.encode({ 1, x = 1 }):sub(1, 1) == "{")
  assert(json.encode({ [0] = 1 }) == '{"0":1}')
  assert(json.encode({ 1, n = 1 }):sub(1, 1) == "{")
  assert(json.encode(json.array({ [1] = 1, [3] = 3 })) == "[1,null,3]")
  assert(json.encode(json.array({ [20] = 1 })) == '{"20":1}')
  local obj = json.encode(json.object({ 1, 2 }))
  assert(obj:sub(1, 1) == "{" and deepeq(json.decode(obj), { ["1"] = 1, ["2"] = 2 }))
end


print("testing json errors and option names")

do
  local big = { string.rep("x", 100000), string.rep("y", 100000), print }
  assert(fails(json.encode, big))
  assert(json.encode({ 1, 2 }) == "[1,2]")
  assert(#json.encode({ string.rep("x", 100000) }) == 100004)
  local seen
  local function handler (reason)
    seen = reason
    return "nan"
  end
  assert(json.encode(vec3(0/0, 1, 2), { nan = false, exception = handler }) == '{"x":"nan","y":1.0,"z":2.0}')
  assert(seen == "error encoding number")
  assert(json.encode(vec2(1/0, 1), { nan = false, vectorarray = true, exception = handler }) == '["nan",1.0]')
  local ok, err = pcall(json.encode, vec3(0/0, 1, 2), { nan = false })
  assert(not ok and err:find("NaN or infinity", 1, true))
  ok, err = pcall(json.encode, { 0/0 }, { nan = false })
  assert(not ok and err:find("NaN or infinity", 1, true))
  assert(json.encode({ json.null, 1 }) == "[null,1]")
  assert(fails(json.encode, { 1 }, { bogus = true }))
  assert(fails(json.encode, { 1 }, { [1] = true }))
  assert(fails(json.encode, { 1 }, { [""] = true }))
  assert(fails(json.setoption, "bogus", true))
  assert(fails(json.setoption, "keyorder", {}))
  assert(fails(json.getoption, "exception"))
  assert(fails(json.setoption, "decoder_preset", "bogus"))
  assert(json.encode({ 1 }, { pretty = false }) == "[1]")
  ok, err = pcall(json.array, 5)
  assert(not ok and err:find("optional table expected", 1, true))
end

print("OK")

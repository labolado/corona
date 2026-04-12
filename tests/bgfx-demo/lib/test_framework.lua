------------------------------------------------------------------------
-- test_framework.lua - Lightweight Lua test framework for Solar2D
--
-- Usage:
--   local T = require("lib.test_framework")
--   T.describe("group name", function()
--       T.it("test case", function()
--           T.assertEqual(1 + 1, 2)
--       end)
--   end)
--   T.run()
--
-- Output format (grep-friendly):
--   [PASS] group > test case
--   [FAIL] group > test case: expected 3, got 2
--   [TEST_SUMMARY] total=N passed=N failed=N
------------------------------------------------------------------------

local T = {}
T._suites = {}
T._passed = 0
T._failed = 0
T._errors = {}
T._currentDescribe = nil

------------------------------------------------------------------------
-- Structure
------------------------------------------------------------------------

function T.describe(name, fn)
    T._currentDescribe = name
    fn()
    T._currentDescribe = nil
end

function T.it(name, fn)
    local fullName = T._currentDescribe and (T._currentDescribe .. " > " .. name) or name
    local ok, err = pcall(fn)
    if ok then
        T._passed = T._passed + 1
        print("[PASS] " .. fullName)
    else
        T._failed = T._failed + 1
        print("[FAIL] " .. fullName .. ": " .. tostring(err))
        T._errors[#T._errors + 1] = { name = fullName, err = tostring(err) }
    end
end

------------------------------------------------------------------------
-- Assertions
------------------------------------------------------------------------

local function fail(msg)
    error(msg, 3)
end

function T.assertEqual(actual, expected, msg)
    if actual ~= expected then
        fail((msg or "assertEqual") .. " - expected " .. tostring(expected) .. ", got " .. tostring(actual))
    end
end

function T.assertNotEqual(actual, expected, msg)
    if actual == expected then
        fail((msg or "assertNotEqual") .. " - values are equal: " .. tostring(actual))
    end
end

function T.assertTrue(value, msg)
    if not value then
        fail((msg or "assertTrue") .. " - value is falsy: " .. tostring(value))
    end
end

function T.assertFalse(value, msg)
    if value then
        fail((msg or "assertFalse") .. " - value is truthy: " .. tostring(value))
    end
end

function T.assertNil(value, msg)
    if value ~= nil then
        fail((msg or "assertNil") .. " - expected nil, got " .. tostring(value))
    end
end

function T.assertNotNil(value, msg)
    if value == nil then
        fail((msg or "assertNotNil") .. " - value is nil")
    end
end

function T.assertAlmostEqual(actual, expected, tolerance, msg)
    tolerance = tolerance or 0.001
    if type(actual) ~= "number" or type(expected) ~= "number" then
        fail((msg or "assertAlmostEqual") .. " - non-numeric: " .. tostring(actual) .. ", " .. tostring(expected))
    end
    if math.abs(actual - expected) > tolerance then
        fail((msg or "assertAlmostEqual") .. " - expected ~" .. tostring(expected) .. " (tol " .. tolerance .. "), got " .. tostring(actual))
    end
end

function T.assertError(fn, msg)
    local ok, _ = pcall(fn)
    if ok then
        fail((msg or "assertError") .. " - expected error but function succeeded")
    end
end

function T.assertType(value, expectedType, msg)
    if type(value) ~= expectedType then
        fail((msg or "assertType") .. " - expected type " .. expectedType .. ", got " .. type(value))
    end
end

------------------------------------------------------------------------
-- Runner
------------------------------------------------------------------------

function T.run()
    local total = T._passed + T._failed
    print("")
    print("[TEST_SUMMARY] total=" .. total .. " passed=" .. T._passed .. " failed=" .. T._failed)

    if T._failed > 0 then
        print("")
        print("--- Failed Tests ---")
        for _, e in ipairs(T._errors) do
            print("  " .. e.name .. ": " .. e.err)
        end
    end

    print("")
    if T._failed == 0 then
        print("ALL " .. total .. " TESTS PASSED")
    else
        print(T._failed .. " of " .. total .. " TESTS FAILED")
    end

    -- Exit simulator after a short delay to let logs flush
    timer.performWithDelay(800, function()
        if native and native.requestExit then
            native.requestExit()
        elseif os.exit then
            os.exit(T._failed > 0 and 1 or 0)
        end
    end)
end

return T

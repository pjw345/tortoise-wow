-- Matching client data patch + server-side rod hierarchy are required.
-- Keep native crafting, reagents, target selection and trade handling.
if WarbandEnchantingRods then return end
WarbandEnchantingRods = {}
local M = WarbandEnchantingRods
local D = WarbandEnchantingRodsData
if not D or D.version ~= 1 then return end
local nativeFocus, nativeCraft
local function Pack(...) return arg end
local function Recipe(index)
  if type(index) ~= "number" or index < 1 then return end
  if type(GetCraftItemLink) == "function" then
    local link = GetCraftItemLink(index)
    local _, _, id = string.find(link or "", "enchant:(%d+)")
    if id and D.recipes[tonumber(id)] then return D.recipes[tonumber(id)] end
  end
  if type(GetCraftInfo) ~= "function" then return end
  local name, rank, kind = GetCraftInfo(index)
  if not name or kind == "header" then return end
  local id = D.byName[name.."\031"..(rank or "")]
  return id and D.recipes[id]
end
function M.HasRod(tier)
  for bag = 0, 4 do
    for slot = 1, GetContainerNumSlots(bag) do
      local link = GetContainerItemLink(bag, slot)
      local _, _, id = string.find(link or "", "item:(%d+)")
      id = tonumber(id)
      if id then
        for level = tier, table.getn(D.rods) do
          if id == D.rods[level] then return true end
        end
      end
    end
  end
  return false
end
local function RodName(tier)
  local name = GetItemInfo(D.rods[tier])
  return name or D.rodNames[tier]
end
local function PatchActive(required, focus)
  -- If the original exact-rod focus is still exposed, keep native behaviour.
  -- This prevents addon-only installation from advertising a working patch.
  for _, tier in ipairs(required) do
    local name = RodName(tier)
    for i = 1, focus.n, 2 do
      if focus[i] == name or focus[i] == D.rodNames[tier] then return false end
    end
  end
  return true
end
local function Requirements(index)
  local required = Recipe(index)
  if not required then return end
  local focus = Pack(nativeFocus(index))
  if not PatchActive(required, focus) then return end
  return required, focus
end
function M.Install()
  if nativeCraft or type(DoCraft) ~= "function" or type(GetCraftSpellFocus) ~= "function" then return end
  nativeCraft, nativeFocus = DoCraft, GetCraftSpellFocus
  GetCraftSpellFocus = function(index)
    local required, focus = Requirements(index)
    if not required then return nativeFocus(index) end
    -- Native APIs may return one nil value instead of zero values when no
    -- focus remains. Keep complete name/ownership pairs before adding ours.
    local native = focus
    focus = { n = 0 }
    for i = 1, native.n, 2 do
      if native[i] and native[i] ~= "" then
        focus.n = focus.n + 2
        focus[focus.n-1], focus[focus.n] = native[i], native[i+1]
      end
    end
    for _, tier in ipairs(required) do
      focus.n = focus.n + 2
      focus[focus.n-1] = RodName(tier).." (or higher)"
      focus[focus.n] = M.HasRod(tier) and 1 or nil
    end
    return unpack(focus)
  end
  DoCraft = function(index, ...)
    local required = Requirements(index)
    if required then
      for _, tier in ipairs(required) do
        if not M.HasRod(tier) then
          if UIErrorsFrame then UIErrorsFrame:AddMessage("Requires "..RodName(tier).." or higher", 1, 0.1, 0.1, 1) end
          return
        end
      end
    end
    return nativeCraft(index, unpack(arg))
  end
end
M.Install()
local frame = CreateFrame("Frame")
frame:RegisterEvent("ADDON_LOADED")
frame:RegisterEvent("CRAFT_SHOW")
frame:SetScript("OnEvent", function() M.Install() end)
SLASH_WARBANDENCHANTINGRODS1 = "/werods"
SlashCmdList.WARBANDENCHANTINGRODS = function()
  local index = GetCraftSelectionIndex and GetCraftSelectionIndex()
  local required = index and Recipe(index)
  local text = "Select an enchanting recipe to check the rod patch."
  if required and nativeFocus then
    if PatchActive(required, Pack(nativeFocus(index))) then
      text = "Enchanting rod patch is active for this recipe."
    else
      text = "Original rod requirement is active. Install the matching patch-Z.MPQ and restart the client."
    end
  end
  DEFAULT_CHAT_FRAME:AddMessage(text)
end

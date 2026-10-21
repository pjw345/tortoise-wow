local P = pfQuestPartyBots
local panel, firstRow, firstMember = nil, 1, 1

local function PlayerRecord(id)
  for key, quest in pairs((pfQuest and pfQuest.questlog) or {}) do
    if P.QuestId(key, quest) == id then
      local title, level, tag, header, collapsed, complete = GetQuestLogTitle(quest.qlogid)
      if title == quest.title then return { id = id, title = title, level = level, state = complete == -1 and "F" or complete and "C" or "I" } end
    end
  end
end

function P.ActiveQuests()
  local quests = {}
  for _, id in ipairs(P.PlayerQuestIds()) do
    local record = PlayerRecord(id)
    if record then quests[id] = record end
  end
  for _, member in ipairs(P.roster) do
    local snapshot = P.snapshots[member.name]
    for id, record in pairs(snapshot and snapshot.records or {}) do
      if record.state == "I" or record.state == "C" or record.state == "F" then quests[id] = quests[id] or record end
    end
  end
  local result = {}
  for _, quest in pairs(quests) do table.insert(result, quest) end
  table.sort(result, function(a, b) if a.title == b.title then return a.id < b.id end return a.title < b.title end)
  return result
end

local function Button(name, text, x, y, width, click)
  local button = CreateFrame("Button", name, panel, "UIPanelButtonTemplate")
  button:SetPoint("TOPLEFT", panel, "TOPLEFT", x, y); button:SetWidth(width); button:SetHeight(22)
  button:SetText(text); button:SetScript("OnClick", click)
  return button
end

function P.RefreshPanel()
  if not panel or not panel:IsShown() then return end
  local quests = P.ActiveQuests()
  firstRow = math.max(1, math.min(firstRow, math.max(1, table.getn(quests) - 11)))
  firstMember = math.max(1, math.min(firstMember, math.max(1, table.getn(P.roster) - 3)))
  local names = { UnitName("player") .. " (you)" }
  for i = firstMember, firstMember + 3 do table.insert(names, P.roster[i] and P.roster[i].name or "") end
  for i = 1, 5 do panel.headers[i]:SetText(names[i]) end
  for i, row in ipairs(panel.rows) do
    local quest = quests[firstRow + i - 1]
    row:Hide()
    if quest then
      row.questId, row.questTitle = quest.id, quest.title
      row.title:SetText(quest.title)
      local own = PlayerRecord(quest.id)
      row.cells[1]:SetText(P.labels[own and own.state or "N"])
      for column = 2, 5 do
        local member = P.roster[firstMember + column - 2]
        if member then
          local label, record, stale = P.Status(member.name, quest.id)
          row.cells[column]:SetText(label .. (stale and "*" or ""))
        else row.cells[column]:SetText("") end
      end
      row:Show()
    end
  end
  panel.info:SetText(table.getn(quests) .. " party quests. * stale/unavailable. Not in log does not establish a turn-in.")
end

local function CreatePanel()
  panel = CreateFrame("Frame", "pfQuestPartyBotsPanel", UIParent)
  panel:SetWidth(820); panel:SetHeight(435); panel:SetPoint("CENTER", UIParent, "CENTER", 0, 0)
  panel:SetFrameStrata("DIALOG"); panel:EnableMouse(true); panel:SetMovable(true); panel:RegisterForDrag("LeftButton")
  panel:SetClampedToScreen(true)
  panel:SetBackdrop({ bgFile = "Interface\\DialogFrame\\UI-DialogBox-Background", edgeFile = "Interface\\DialogFrame\\UI-DialogBox-Border",
    tile = true, tileSize = 32, edgeSize = 16, insets = { left = 5, right = 5, top = 5, bottom = 5 } })
  panel:SetScript("OnDragStart", function() this:StartMoving() end)
  panel:SetScript("OnDragStop", function() this:StopMovingOrSizing() end)
  panel.title = panel:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
  panel.title:SetPoint("TOPLEFT", panel, "TOPLEFT", 18, -15); panel.title:SetText("Party quest progress")
  local close = CreateFrame("Button", nil, panel, "UIPanelCloseButton"); close:SetPoint("TOPRIGHT", panel, "TOPRIGHT", -3, -3)
  Button(nil, "Refresh", 18, -42, 90, function() P.Refresh() end)
  Button(nil, "Earlier quests", 120, -42, 105, function() firstRow = math.max(1, firstRow - 12); P.RefreshPanel() end)
  Button(nil, "Later quests", 230, -42, 105, function() firstRow = firstRow + 12; P.RefreshPanel() end)
  Button(nil, "Earlier bots", 590, -42, 105, function() firstMember = math.max(1, firstMember - 4); P.RefreshPanel() end)
  Button(nil, "Later bots", 700, -42, 100, function() firstMember = firstMember + 4; P.RefreshPanel() end)
  panel.headers, panel.rows = {}, {}
  local title = panel:CreateFontString(nil, "OVERLAY", "GameFontNormal")
  title:SetPoint("TOPLEFT", panel, "TOPLEFT", 18, -78); title:SetText("Quest")
  for i = 1, 5 do
    local header = panel:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    header:SetPoint("TOPLEFT", panel, "TOPLEFT", 300 + (i - 1) * 100, -78); header:SetWidth(98); header:SetJustifyH("CENTER")
    panel.headers[i] = header
  end
  for i = 1, 12 do
    local row = CreateFrame("Button", nil, panel)
    row:SetPoint("TOPLEFT", panel, "TOPLEFT", 18, -97 - (i - 1) * 24); row:SetWidth(782); row:SetHeight(24)
    row.title = row:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
    row.title:SetPoint("LEFT", row, "LEFT", 0, 0); row.title:SetWidth(275); row.title:SetJustifyH("LEFT")
    row.cells = {}
    for column = 1, 5 do
      local cell = row:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
      cell:SetPoint("LEFT", row, "LEFT", 282 + (column - 1) * 100, 0); cell:SetWidth(98); cell:SetJustifyH("CENTER")
      row.cells[column] = cell
    end
    row:SetScript("OnEnter", function()
      GameTooltip:SetOwner(this, "ANCHOR_RIGHT"); GameTooltip:SetText(this.questTitle .. " (" .. this.questId .. ")"); P.AddTooltip(this.questId)
    end)
    row:SetScript("OnLeave", function() GameTooltip:Hide() end)
    panel.rows[i] = row
  end
  panel.info = panel:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
  panel.info:SetPoint("BOTTOMLEFT", panel, "BOTTOMLEFT", 18, 18)
  panel:SetScript("OnShow", function() P.RefreshPanel(); P.Refresh() end)
  table.insert(UISpecialFrames, "pfQuestPartyBotsPanel")
end

function P.TogglePanel()
  if not panel then CreatePanel(); panel:Hide() end
  if panel:IsShown() then panel:Hide() else panel:Show(); P.RefreshPanel() end
end
table.insert(P.listeners, P.RefreshPanel)
local frame = CreateFrame("Frame")
frame:RegisterEvent("PLAYER_LOGIN")
frame:SetScript("OnEvent", function()
  if QuestLogFrame then
    local button = CreateFrame("Button", "pfQuestPartyBotsQuestLogButton", QuestLogFrame, "UIPanelButtonTemplate")
    button:SetPoint("TOPLEFT", QuestLogFrame, "TOPRIGHT", -8, -30); button:SetWidth(108); button:SetHeight(22)
    button:SetText("Party quests"); button:SetScript("OnClick", P.TogglePanel)
  end
end)
SLASH_PFQUESTPARTYBOTS1 = "/pfqb"
SlashCmdList["PFQUESTPARTYBOTS"] = function(text)
  if text == "refresh" then P.Refresh() else P.TogglePanel() end
end

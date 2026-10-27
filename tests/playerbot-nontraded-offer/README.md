Extracts production TradeAction::Execute/TradeItem, native begin/item/clear/gold
handlers, and native TradeData::SetItem. The fixture controls initiation outcomes,
inventory parsing, player lookup, sessions and packets. It exercises first-command
window/item ordering, explicit nt on transferable and soulbound/equipped items,
repetition, occupied slots, missing items, native bank/duplicate guards, intended
partner/busy checks, normal counted/money trading, and inventory changes during
the native initiation hook boundary. Other core variants keep their initiate/ack
flow while exercising native item placement. TRADE_SOURCE supplies deployed code
for separate first-command and prefilled-slot negative controls.

The fixture does not execute the full native initiation eligibility/script engine,
socket packet exchange, client window rendering or completed trade enchanting.
No accept/completion handler is exposed to the action. Live acceptance must check
the client displays the offered item, enchanting succeeds on the same owned item,
and cancellation/rejection consumes no reagents or inventory.

<< Data records, no classes/methods: money is represented in integer cents.
def Entry { string Account; number Cents; string Reference; }
array<Entry> ledger = {
  Entry { Account = "cash", Cents = 12500, Reference = "sale-1" },
  Entry { Account = "revenue", Cents = -12500, Reference = "sale-1" },
  Entry { Account = "expense", Cents = 3200, Reference = "fee-1" },
  Entry { Account = "cash", Cents = -3200, Reference = "fee-1" }
};
number Balance(array<Entry> entries, string account) {
  number balance = 0;
  for (Entry entry in entries) { if (entry.Account == account) { balance += entry.Cents; } }
  return balance;
}
number total = 0;
for (Entry entry in ledger) { total += entry.Cents; }
if (total != 0) { report("unbalanced ledger"); }
post("balanced:" .: (total == 0));
post("cash:" .: Balance(ledger, "cash"));
post("revenue:" .: Balance(ledger, "revenue"));
post("expense:" .: Balance(ledger, "expense"));

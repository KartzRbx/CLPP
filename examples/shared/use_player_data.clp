// The original link syntax binds the module namespace.
link "./PlayerData.clh" as Data;

void Demo() {
    auto wallet = Data.Wallet.new_();
    wallet.Coins = 10;
    auto data = Data.PlayerData.new_();
    data.Currencies.Coins = wallet.Coins;
}

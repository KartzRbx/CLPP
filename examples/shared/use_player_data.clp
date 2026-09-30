link "./PlayerData.clh" as PlayerData;

void Demo() {
    Wallet w;
    w.Coins = 10;
    PlayerData d;
    d.Currencies.Coins = w.Coins;
}

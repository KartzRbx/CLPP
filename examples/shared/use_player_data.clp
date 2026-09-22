// Language-only consumer: named import (not #include) for modules.
import { Wallet, PlayerData as Data } from "./PlayerData.clh";

void Demo() {
    Wallet w;
    w.Coins = 10;
    Data d;
    d.Currencies.Coins = w.Coins;
}

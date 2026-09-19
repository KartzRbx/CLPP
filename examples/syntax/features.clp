void Features(int coins) {
    int n = 100;
    float speed = 16.5;
    string name = "Kartz";
    bool isActive = true;
    func callback = func() {};
    auto playerRef = null;

    array<string> names = {"Kartz", "Player1"};
    dictionary<string, int> stats = {
        {"Coins", 100},
        {"Gems", 50}
    };

    if (coins > 50) {
        post("Enough balance!");
    } else if (coins == 0) {
        warn("No coins!");
    } else {
        report("Balance sync error.");
    }

    for (int i = 0; i < 10; i++) {
        post("Count: " .: i);
    }

    n += 1;

    observable int wallet = 100;
    wallet.OnChange(func (int newValue) {
        post("Coins changed to: " .: newValue);
    });

    signal<Player*, int> OnCoinsUpdated;
    OnCoinsUpdated~>Connect(func (Player* player, int newAmount) {
        post("New coins for " .: player.Name .: ": " .: newAmount);
    });
    OnCoinsUpdated~>Once(func (Player* player, int newAmount) {
        post("First coins for " .: player.Name);
    });
    OnCoinsUpdated.Fire(playerRef, wallet);

    playerRef.GetPropertyChangedSignal("Name")~>Connect(func () {
        post("Name changed");
    });

    string title = 'Player';
    post(`PlayerName is {playerRef.Name}`);
    post(`PlayerName is `, playerRef.Name, `.`);

    guard (coins != null) else {
        warn("Invalid player");
        return;
    }

    auto [success, result] = pcall(func () {
        return 1;
    });

    const int moeda = 4;

    match (moeda) {
        int a => post(a),
        _ => post("Not found")
    }; 

    if (success) {
        post("Data loaded successfully!");
    }

    spawn {
        task::wait(2);
        post("Delay finished!");
    };

    parallel {
        post("desync");
    };

    match (name) {
        string s => post(s),
        _ => warn("Instance not supported")
    };
}

async Data* FetchData(Player* player) {
    Data* data = await DataService.Server.WaitFor(player);
    return data;
}

[[server]]
void SaveData(Player* player) {
    post(player.Name);
}

[[client]]
void UpdateUI() {
    post("ui");
}

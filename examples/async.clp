<< Tarefas: spawn cria a tarefa, await espera o resultado
async number dobro(number x) {
    return x * 2;
}

task<number> job = spawn dobro(21);
post(await job);

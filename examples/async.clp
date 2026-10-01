<< Tarefas: spawn cria a tarefa, await espera o resultado
async func dobro(x) -> int {
    return x * 2;
}

task job = spawn dobro(21);
post(await job);

void UpdatePosition(Vector3 velocity) {
    Vector3 currentPos = Vector3(0.0, 10.0, 0.0);
    Vector3 nextPos = currentPos + (velocity * 0.016);
    << math nativa via opcode veloz

    post("New Pos: " .: nextPos.x .: ", " .: nextPos.y .: ", " .: nextPos.z);
}


void ProcessPacket() {
   

    buffer stream = buffer::create(256);
    buffer::write_string(stream, 0, "CL++ Data");

    post("Buffer allocated with size: " .: buffer::size(stream));
}

<< @coins, @this.janitor e @this::BindPart referem-se à mesma tabela (self)
ProcessPacket();
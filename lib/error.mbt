pub enum RedisError {
  WrongType
  NotInteger
  NotFound
  InvalidArgument
  Overflow
}

pub fn RedisError::message(self : RedisError) -> String {
  match self {
    RedisError::WrongType => "WRONGTYPE Operation against a key holding the wrong kind of value"
    RedisError::NotInteger => "ERR value is not an integer or out of range"
    RedisError::NotFound => "ERR no such key"
    RedisError::InvalidArgument => "ERR invalid argument"
    RedisError::Overflow => "ERR increment or decrement would overflow"
  }
}
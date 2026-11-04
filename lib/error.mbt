pub enum RedisError {
  WrongType
  NotInteger
  NotFound
  InvalidArgument
  OutOfRange
  IndexOutOfRange
  SyntaxError
  BusyKey
  WrongArgumentCount(String)
  Overflow
  TransactionAborted
}

pub fn RedisError::message(self : RedisError) -> String {
  match self {
    RedisError::WrongType => "WRONGTYPE Operation against a key holding the wrong kind of value"
    RedisError::NotInteger => "ERR value is not an integer or out of range"
    RedisError::NotFound => "ERR no such key"
    RedisError::InvalidArgument => "ERR invalid argument"
    RedisError::OutOfRange => "ERR value is out of range, must be positive"
    RedisError::IndexOutOfRange => "ERR index out of range"
    RedisError::SyntaxError => "ERR syntax error"
    RedisError::BusyKey => "BUSYKEY Target key name already exists."
    RedisError::WrongArgumentCount(cmd) => "ERR wrong number of arguments for '\{cmd}' command"
    RedisError::Overflow => "ERR increment or decrement would overflow"
    RedisError::TransactionAborted => "TransactionAborted Transaction aborted because the watched keys were modified"
  }
}
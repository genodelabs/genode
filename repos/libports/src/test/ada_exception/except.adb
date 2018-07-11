package body Except is

    procedure Raise_Task
    is
    begin
        raise Program_Error;
    end Raise_Task;

end Except;

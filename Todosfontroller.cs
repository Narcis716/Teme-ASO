using Microsoft.AspNetCore.Mvc;
using Microsoft.EntityFrameworkCore;
using TodoApp.Data;
using TodoApp.Models;

namespace TodoApp.Controllers;

[ApiController]
[Route("api/[controller]")]
public class TodosController : ControllerBase
{
    private readonly TodoDbContext _context;

    public TodosController(TodoDbContext context)
    {
        _context = context;
    }

    [HttpGet]
    public async Task<ActionResult<IEnumerable<Todo>>> GetAll()
    {
        return await _context.Todos.OrderByDescending(t => t.CreatedAt).ToListAsync();
    }

    [HttpGet("{id}")]
    public async Task<ActionResult<Todo>> GetById(int id)
    {
        var todo = await _context.Todos.FindAsync(id);
        if (todo == null) return NotFound();
        return todo;
    }

    [HttpPost]
    public async Task<ActionResult<Todo>> Create(Todo todo)
    {
        todo.CreatedAt = DateTime.UtcNow;
        _context.Todos.Add(todo);
        await _context.SaveChangesAsync();
        return CreatedAtAction(nameof(GetById), new { id = todo.Id }, todo);
    }

    [HttpPut("{id}")]
    public async Task<IActionResult> Update(int id, Todo updated)
    {
        if (id != updated.Id) return BadRequest();
        _context.Entry(updated).State = EntityState.Modified;
        await _context.SaveChangesAsync();
        return NoContent();
    }

    [HttpPatch("{id}/toggle")]
    public async Task<ActionResult<Todo>> Toggle(int id)
    {
        var todo = await _context.Todos.FindAsync(id);
        if (todo == null) return NotFound();
        todo.IsCompleted = !todo.IsCompleted;
        await _context.SaveChangesAsync();
        return todo;
    }

    [HttpDelete("{id}")]
    public async Task<IActionResult> Delete(int id)
    {
        var todo = await _context.Todos.FindAsync(id);
        if (todo == null) return NotFound();
        _context.Todos.Remove(todo);
        await _context.SaveChangesAsync();
        return NoContent();
    }
}